#!/usr/bin/env bash
# Starts local Mosquitto MQTT broker if installed

if command -v mosquitto >/dev/null 2>&1; then
    echo "[INFO] Starting Mosquitto MQTT broker in background on port 1883..."
    mosquitto -v -p 1883 > logs/mosquitto.log 2>&1 &
    echo $! > mosquitto.pid
    echo "[INFO] Mosquitto running (PID: $(cat mosquitto.pid))"
else
    echo "[NOTICE] Mosquitto broker not found in PATH."
    echo "[NOTICE] Gateway will run in autonomous mode without external broker."
fi
