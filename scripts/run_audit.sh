#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build-release --target popx -j 2
.venv/bin/python -m dashboard.diagnostics audit "${1:-/tmp/popx_audit.sqlite}" "${2:-2000}"
