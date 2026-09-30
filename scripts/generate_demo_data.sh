#!/usr/bin/env bash
# Automatically runs a cycle of telemetry monitoring and fault injection for demonstrations

echo "======================================================="
echo " Industrial Edge Controller - Demonstration Sequence"
echo "======================================================="

echo "[1/5] Querying initial nominal SCADA registers via Modbus TCP..."
python3 tools/modbus_client.py --port 1502

sleep 2

echo ""
echo "[2/5] Querying CLI status via TCP Port 9100..."
python3 tools/tcp_monitor.py --cmd STATUS

sleep 2

echo ""
echo "[3/5] Injecting High Temperature Fault (>85°C)..."
python3 tools/fault_injector.py --temperature-high

sleep 2

echo ""
echo "[4/5] Reading SCADA registers after EMERGENCY_STOP trip..."
python3 tools/modbus_client.py --port 1502

sleep 2

echo ""
echo "[5/5] Resetting fault and recovering actuator..."
python3 tools/fault_injector.py --reset
python3 tools/tcp_monitor.py --cmd START

echo ""
echo "Demo automated sequence completed successfully."
