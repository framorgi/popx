#!/usr/bin/env bash
# Standalone invariants, without invoking the source-mutating format ALL target.
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --build build-release --target popx -j 2
for regression in respiration pop_cycle parallel_phase genome_encoding pheromone stats_runtime; do
    clang++ -std=c++23 -O2 -Isrc \
        -Ibuild-release/_deps/eigen3-src -Ibuild-release/_deps/nlohmann_json-src/include \
        "tests/${regression}_regression.cpp" \
        build-release/src/CMakeFiles/popx.dir/{brain,cell,config,console_logger,gene,genome,grid_world,pop,pops_manager,random_utility,simulator,stats}.cpp.o \
        -ltbb -lstdc++fs -o "/tmp/popx_${regression}_regression"
    "/tmp/popx_${regression}_regression"
done
