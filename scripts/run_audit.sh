#!/usr/bin/env bash
# Build/run the short headless audit without the source-mutating format ALL target.
set -euo pipefail
cd "$(dirname "$0")/.."
output_path=${1:-/tmp/popx_audit.json}
audit_ticks=${2:-2000}
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O1 -Isrc \
    -Ibuild-release/_deps/eigen3-src \
    -Ibuild-release/_deps/nlohmann_json-src/include \
    tools/popx_audit.cpp \
    build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility,simulator}.cpp.o \
    -ltbb -lstdc++fs -o /tmp/popx_audit
mkdir -p "$(dirname "$output_path")"
/tmp/popx_audit config/popx.ini "$output_path" "$audit_ticks"
