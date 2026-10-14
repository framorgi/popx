#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build-release --target popx -j 2
.venv/bin/python -m dashboard.diagnostics strategy "${1:-/tmp/popx_strategy.sqlite}" "${3:-all}" --corpus "${2:-/tmp/popx_audit.sqlite}"
