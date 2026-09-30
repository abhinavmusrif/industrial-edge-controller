#!/usr/bin/env bash
set -e

echo "======================================================="
echo " Building Industrial Edge Controller & Digital Twin"
echo "======================================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 1. Check dependencies
chmod +x scripts/*.sh || true
./scripts/check_dependencies.sh

# 2. Configure build directory
mkdir -p build
mkdir -p logs

# If CMakeCache was created on Windows with drive letters, clean it for Linux
if grep -q "[A-Za-z]:/" build/CMakeCache.txt 2>/dev/null; then
    echo "[INFO] Cleaning cross-platform Windows CMake cache for Linux build..."
    rm -rf build/*
fi

echo ""
echo "[INFO] Running CMake configuration..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# 3. Compile targets
echo ""
echo "[INFO] Compiling C++ targets..."
cmake --build build --config Release -j$(nproc 2>/dev/null || echo 2)

# 4. Run automated test suite
echo ""
echo "[INFO] Executing automated unit test suite..."
if [ -f "./build/bin/run_tests" ]; then
    ./build/bin/run_tests
elif [ -f "./build/run_tests" ]; then
    ./build/run_tests
elif [ -f "./build/Release/run_tests.exe" ]; then
    ./build/Release/run_tests.exe
else
    echo "[WARN] Test binary location not identified directly; running ctest..."
    (cd build && ctest --output-on-failure)
fi

echo ""
echo "======================================================="
echo " BUILD SUCCESSFUL!"
echo " Executables generated in build/bin/:"
echo "   - edge_gateway"
echo "   - mcu_simulator"
echo "   - run_tests"
echo "======================================================="
