"""Bounded production scheduler pilot; separate random runs are not a causal benchmark."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from fastapi.testclient import TestClient
from dashboard.server import ROOT, create_app
from tests.dashboard_integration import free_port


def run_mode(folder, visible):
    mode = 'gui' if visible else 'headless'
    config = folder / f'{mode}.ini'
    # Only duration differs from production configuration. No physiology/policy overrides.
    config.write_text((ROOT / 'config/popx.ini').read_text() + '\nmax_duration_minutes=0.3333333333333333\n')
    port = free_port()
    db = folder / f'{mode}.sqlite'
    with TestClient(create_app(db, config, ROOT / 'build-release/src/popx', port)) as client:
        argv = [str(ROOT / 'build-release/src/popx'), '--config', str(config), '--udp-port', str(port), '--speed', '2']
        if not visible:
            argv.append('--headless')
        proc = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        began = time.monotonic()
        try:
            while time.monotonic() - began < 30:
                status = client.get('/api/status').json()
                if proc.poll() is not None:
                    raise RuntimeError(proc.communicate())
                if status.get('state') == 'finished':
                    break
                time.sleep(.1)
            else:
                raise RuntimeError('30 second pilot budget exceeded')
            assert status['tick'] <= 2000, status
            assert status['reason'] in ('duration', 'extinction'), status
            if status['reason'] == 'duration':
                assert status['tick'] == 2000, status
            result = {'mode': mode, 'wall_seconds': time.monotonic() - began,
                      'tick': status['tick'], 'reason': status['reason'], 'alive': status['alive'],
                      'performance': status['performance'], 'database': str(db)}
            response = client.post('/api/commands', json={'command': 'exit', 'run_id': status['run_id']})
            assert response.status_code == 200, response.text
            proc.wait(timeout=5)
            assert proc.returncode == 0, proc.stderr.read().decode()
            print(json.dumps(result), flush=True)
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=5)
            proc.stdout.close()
            proc.stderr.close()


def main():
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    if not os.environ.get('DISPLAY'):
        raise SystemExit('DISPLAY required for the GUI comparison')
    args.output.mkdir(parents=True, exist_ok=True)
    run_mode(args.output.resolve(), False)
    run_mode(args.output.resolve(), True)


if __name__ == '__main__':
    main()
