#!/usr/bin/env bash
# Launch only owned processes; ordinary startup never installs or builds.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
config="$root/config/popx.ini"
binary="$root/build-release/src/popx"
database="$root/data_out/popx.sqlite"
http_port=8000
udp_port=8001
speed=1
headless=0
setup=0
build=0
while (($#)); do
    case "$1" in
        --help) cat <<'HELP'
Usage: scripts/run_popx.sh [options]
  --headless          Start without a graphical window
  --config PATH       Simulation INI (default: config/popx.ini)
  --binary PATH       PopX executable (default: build-release/src/popx)
  --db PATH           SQLite history (default: data_out/popx.sqlite)
  --http-port PORT    Local dashboard port (default: 8000)
  --udp-port PORT     Local telemetry/control port (default: 8001)
  --speed 1|2         Initial speed, 100 or 200 ticks/s
  --setup             Install pinned dependencies in .venv and local web assets
  --build             Build the explicit popx Release target
  --help              Show this help
Stop ends a run; Exit or Ctrl-C closes the launcher and both owned processes.
Headless can reopen the GUI from the dashboard when a display is available.
HELP
            exit 0 ;;
        --headless) headless=1; shift ;;
        --setup) setup=1; shift ;;
        --build) build=1; shift ;;
        --config|--binary|--db|--http-port|--udp-port|--speed)
            (($# >= 2)) || { echo "Missing value for $1" >&2; exit 2; }
            case "$1" in
                --config) config=$2;; --binary) binary=$2;; --db) database=$2;;
                --http-port) http_port=$2;; --udp-port) udp_port=$2;; --speed) speed=$2;;
            esac
            shift 2 ;;
        *) echo "Unknown option: $1 (see --help)" >&2; exit 2 ;;
    esac
done
[[ "$speed" == 1 || "$speed" == 2 ]] || { echo 'Speed must be 1 or 2' >&2; exit 2; }
for port in "$http_port" "$udp_port"; do
    [[ "$port" =~ ^[0-9]+$ ]] && ((port > 0 && port < 65536)) || { echo 'Invalid port' >&2; exit 2; }
done
config=$(realpath -m -- "$config")
binary=$(realpath -m -- "$binary")
database=$(realpath -m -- "$database")
if ((setup)); then
    python3 -m venv "$root/.venv"
    "$root/.venv/bin/python" -m pip install -r "$root/dashboard/requirements.txt"
    "$root/.venv/bin/python" "$root/scripts/setup_web_assets.py"
fi
if ((build)); then cmake --build "$root/build-release" --target popx -j 2; fi
[[ -x "$binary" && -x "$root/.venv/bin/python" ]] || { echo 'Missing binary or virtualenv; use --setup --build' >&2; exit 1; }
[[ -s "$root/dashboard/static/vendor/chart.umd.js" && -s "$root/dashboard/static/vendor/cytoscape.min.js" ]] || { echo 'Missing web assets; use --setup' >&2; exit 1; }
"$binary" --validate-config "$config" >/dev/null
backend_pid=''
sim_pid=''
cleanup() {
    trap - EXIT INT TERM
    if [[ -n "$sim_pid" ]] && kill -0 "$sim_pid" 2>/dev/null; then kill -TERM "$sim_pid" 2>/dev/null || true; wait "$sim_pid" || true; fi
    if [[ -n "$backend_pid" ]] && kill -0 "$backend_pid" 2>/dev/null; then kill -TERM "$backend_pid" 2>/dev/null || true; wait "$backend_pid" || true; fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM
cd "$root"
"$root/.venv/bin/python" -m dashboard.server --config "$config" --binary "$binary" --db "$database" --http-port "$http_port" --udp-port "$udp_port" &
backend_pid=$!
"$root/.venv/bin/python" - "$http_port" "$backend_pid" <<'PY'
import json,os,sys,time,urllib.request
for _ in range(100):
    try:
        os.kill(int(sys.argv[2]),0)
        with urllib.request.urlopen('http://127.0.0.1:'+sys.argv[1]+'/api/health',timeout=.2) as response:
            if response.status==200 and json.load(response).get("pid")==int(sys.argv[2]): break
    except (OSError,ValueError): time.sleep(.1)
else: raise SystemExit('Backend startup failed')
PY
kill -0 "$backend_pid"
args=(--config "$config" --udp-port "$udp_port" --speed "$speed")
if ((headless)); then args+=(--headless); fi
"$binary" "${args[@]}" &
sim_pid=$!
printf 'PopX dashboard: http://127.0.0.1:%s\n' "$http_port"
wait -n "$backend_pid" "$sim_pid"
