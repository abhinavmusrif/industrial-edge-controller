#!/usr/bin/env bash
set -e

echo "======================================================="
echo " Checking System Dependencies for Industrial Digital Twin"
echo "======================================================="

CHECK_FAIL=0

check_cmd() {
    if command -v "$1" >/dev/null 2>&1; then
        echo "  [OK] Found $1: $(command -v $1)"
    else
        echo "  [WARN] Missing $1"
        if [ "$2" = "REQUIRED" ]; then
            CHECK_FAIL=1
        fi
    fi
}

echo "Core C++ & Build Tools:"
check_cmd "cmake" "REQUIRED"
check_cmd "g++" "REQUIRED" || check_cmd "clang++" "REQUIRED"
check_cmd "make" "REQUIRED"

echo ""
echo "Python & Automation Tools:"
check_cmd "python3" "REQUIRED"

echo ""
echo "Industrial Networking Tools (Optional):"
check_cmd "mosquitto" "OPTIONAL"
check_cmd "nc" "OPTIONAL"
check_cmd "wireshark" "OPTIONAL" || check_cmd "tshark" "OPTIONAL"

if [ $CHECK_FAIL -ne 0 ]; then
    echo ""
    echo "[ERROR] Missing required build dependencies. Please install cmake and g++."
    exit 1
fi

echo ""
echo "[SUCCESS] All core build dependencies verified."
exit 0
