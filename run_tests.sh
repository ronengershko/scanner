#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Cleaning up before tests ==="
rm -rf scanner-data/

echo ""
echo "=== Building ==="
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

echo ""
echo "=== test_cli ==="
./build/test_cli

echo ""
echo "=== test_scanner ==="
./build/test_scanner

echo ""
echo "=== test_db ==="
./build/test_db

echo ""
echo "=== test_integration ==="
./build/test_integration

echo ""
echo "=== Cleaning up after tests ==="
rm -rf scanner-data/

echo ""
echo "All tests passed."
