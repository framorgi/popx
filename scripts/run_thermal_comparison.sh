#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build-release --target popx -j 2
.venv/bin/python -m dashboard.diagnostics thermal "${1:-/tmp/popx_thermal.sqlite}" "${2:-moderate}"
