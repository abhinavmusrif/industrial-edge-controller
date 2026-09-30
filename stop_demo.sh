#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================================="
echo " Stopping Industrial Edge Controller Demo..."
echo "======================================================="

kill_pid() {
    FILE="$1"
    NAME="$2"
    if [ -f "$FILE" ]; then
        PID=$(cat "$FILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "[INFO] Terminating $NAME (PID: $PID)..."
            kill -15 "$PID" 2>/dev/null || kill -9 "$PID" 2>/dev/null || true
        fi
        rm -f "$FILE"
    fi
}

kill_pid "dashboard.pid" "Web Dashboard"
kill_pid "mcu.pid" "MCU Simulator"
kill_pid "gateway.pid" "Linux Gateway"
kill_pid "mosquitto.pid" "Mosquitto Broker"

# Fallback kill by process name if orphaned
pkill -f "edge_gateway" 2>/dev/null || true
pkill -f "mcu_simulator" 2>/dev/null || true
pkill -f "dashboard/app.py" 2>/dev/null || true

echo "All digital twin simulation processes stopped cleanly."
