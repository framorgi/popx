#!/usr/bin/env bash
# Controlled physiology diagnostics; never changes production temperature parameters.
set -euo pipefail
cd "$(dirname "$0")/.."
output_path=${1:-/tmp/popx_thermal_comparison.json}
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O2 -Isrc \
    -Ibuild-release/_deps/eigen3-src -Ibuild-release/_deps/nlohmann_json-src/include \
    tools/popx_thermal_comparison.cpp \
    build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o \
    -ltbb -lstdc++fs -o /tmp/popx_thermal_comparison
mkdir -p "$(dirname "$output_path")"
/tmp/popx_thermal_comparison config/popx.ini "$output_path" "${2:-legacy}"
python3 - "$output_path" <<'PY'
import hashlib
import json
from pathlib import Path
import subprocess
import sys
p = Path(sys.argv[1])
d = json.loads(p.read_text())
d['metadata'] = {
    'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'working_tree_status': subprocess.check_output(['git', 'status', '--short'], text=True),
    'compiler': subprocess.check_output(['clang++', '--version'], text=True),
    'constants': Path('src/common.h').read_text(),
    'fixture_backend': 'sequential controlled phases and physiology; moderate gardens use production FIFO reproduction, no manager update_cycle',
    'rng': 'Fixed effective fixture state; moderate gardens disable mutation and normalize unused random birth traits. C rand bucket assignments unused. Constructor RNG states are not serialized; exact replay validates measured outputs.',
    'harness_sha256': hashlib.sha256(Path('/tmp/popx_thermal_comparison').read_bytes()).hexdigest(),
    'source_sha256': {str(f): hashlib.sha256(f.read_bytes()).hexdigest()
        for f in list(Path('src').glob('*.*')) + [Path('tools/popx_thermal_comparison.cpp')] if f.is_file()},
    'compile_commands': json.loads(Path('build-release/compile_commands.json').read_text()),
}
p.write_text(json.dumps(d, indent=2) + '\n')
PY
