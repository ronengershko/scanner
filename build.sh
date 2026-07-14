#!/bin/bash
set -e
[ ! -d build ] && cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
