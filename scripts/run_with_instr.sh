#!/bin/bash

cmake -S . -B build && cmake --build build

clang++ -fpass-plugin=./build/DefUseGraph.so tests/test.cpp -x c runtime/logger.c -o build/dump_values.x

export RUNTIME_VALUES_FILE_PATH="$(pwd)/assets/runtime_values/runtime_values.txt"

./build/dump_values.x $@

./build/merged_graph.x assets/dot_files/without_instrumentation.dot assets/runtime_values/runtime_values.txt assets/dot_files/with_instrumentation.dot

dot -Tpng assets/dot_files/with_instrumentation.dot -o assets/images/with_instrumentation.png
