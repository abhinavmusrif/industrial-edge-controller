#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "======================================================="
echo " Starting Industrial Edge Controller Simulation Demo"
echo "======================================================="

# Verify binaries exist, if not build first
if [ ! -f "build/bin/edge_gateway" ] && [ ! -f "build/edge_gateway" ] && [ ! -f "build/Release/edge_gateway.exe" ]; then
    echo "[INFO] Binaries not found. Triggering build first..."
    ./build.sh
fi

# Locate binaries
GW_BIN="./build/bin/edge_gateway"
[ -f "$GW_BIN" ] || GW_BIN="./build/edge_gateway"
[ -f "$GW_BIN" ] || GW_BIN="./build/Release/edge_gateway.exe"

MCU_BIN="./build/bin/mcu_simulator"
[ -f "$MCU_BIN" ] || MCU_BIN="./build/mcu_simulator"
[ -f "$MCU_BIN" ] || MCU_BIN="./build/Release/mcu_simulator.exe"

mkdir -p logs

# Clean up any previously running instances
./stop_demo.sh >/dev/null 2>&1 || true
sleep 1

# 1. Start optional MQTT broker
./scripts/start_broker.sh

# 2. Start Linux Edge Gateway
echo "[INFO] Starting Linux Edge Gateway..."
"$GW_BIN" --config config/gateway.conf > logs/gateway_stdout.log 2>&1 &
echo $! > gateway.pid
echo "  [OK] Gateway running (PID: $(cat gateway.pid))"

sleep 1

# 3. Start MCU Simulator
echo "[INFO] Starting MCU Virtual Sensor Simulator..."
"$MCU_BIN" > logs/mcu_stdout.log 2>&1 &
echo $! > mcu.pid
echo "  [OK] MCU Simulator running (PID: $(cat mcu.pid))"

sleep 1

# 4. Start Web SCADA Dashboard
if command -v python3 >/dev/null 2>&1; then
    echo "[INFO] Starting Web SCADA Dashboard..."
    python3 dashboard/app.py --port 8080 > logs/dashboard.log 2>&1 &
    echo $! > dashboard.pid
    echo "  [OK] Dashboard running (PID: $(cat dashboard.pid))"
fi

echo ""
echo "======================================================="
echo " DIGITAL TWIN SIMULATION RUNNING"
echo "======================================================="
echo "  🌐 Web Dashboard:    http://localhost:8080"
echo "  💻 CLI TCP Monitor:   nc localhost 9100"
echo "  ⚡ Modbus TCP:       localhost:1502 (Holding Regs 40001-40008)"
echo "  📡 MCU Link:         localhost:9000 (Binary Protocol + CRC-32)"
echo "  🧰 Fault Injector:   python3 tools/fault_injector.py --temperature-high"
echo "  📋 System Logs:      logs/gateway.log"
echo "======================================================="
echo "To shut down cleanly: ./stop_demo.sh"
