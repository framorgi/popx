#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
output_path=${1:-/tmp/strategy_comparison.json}
corpus_path=${2:-data_out/audit/corrected_pilot.json}
cmake --build build-release --target popx -j 2
clang++ -std=c++23 -O2 -Isrc -Ibuild-release/_deps/eigen3-src -Ibuild-release/_deps/nlohmann_json-src/include \
    tools/popx_strategy_comparison.cpp \
    build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility}.cpp.o \
    -ltbb -lstdc++fs -o /tmp/popx_strategy_comparison
mkdir -p "$(dirname "$output_path")"
/tmp/popx_strategy_comparison "$corpus_path" "$output_path"
