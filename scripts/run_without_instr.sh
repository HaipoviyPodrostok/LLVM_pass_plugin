#!/bin/bash

cmake -S . -B build && cmake --build build

clang++ -O0 -S -emit-llvm tests/test.cpp -o build/test.ll

opt -load-pass-plugin ./build/DefUseGraph.so -passes="def-use-graph" build/test.ll -disable-output

dot -Tpng assets/dot_files/without_instrumentation.dot -o assets/images/without_instrumentation.png
