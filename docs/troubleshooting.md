# Troubleshooting & Diagnostic Guide

## 1. Port Conflicts

The simulation binds to the following local TCP ports:
- **Port 9000:** MCU-to-Gateway virtual serial transport
- **Port 9001:** MCU live fault injection server
- **Port 9100:** CLI monitoring text server
- **Port 1502:** Modbus TCP SCADA server
- **Port 1883:** Mosquitto MQTT broker (optional)
- **Port 8080:** Web SCADA Dashboard HTTP server

### Diagnosis
If a service fails to bind (`Address already in use` or `WSAEADDRINUSE`):
- **On Linux:**
  ```bash
  sudo lsof -i :9000,9001,9100,1502,8080
  sudo netstat -tulpn | grep -E "9000|9100|1502|8080"
  ```
- **On Windows:**
  ```powershell
  Get-NetTCPConnection -LocalPort 9000,9100,1502,8080 -ErrorAction SilentlyContinue
  ```

### Resolution
1. Run `./stop_demo.sh` (or `.\stop_demo.bat`).
2. If another process is using the port, edit `config/gateway.conf` to change `monitor_port` or `mcu_port` to an open port (e.g. 9101, 9002).

---

## 2. Process Cleanup (Orphaned Instances)

If the simulation was interrupted with `Ctrl+C` or a terminal was closed without stopping:

- **Linux / WSL:**
  ```bash
  pkill -9 -f "edge_gateway"
  pkill -9 -f "mcu_simulator"
  pkill -9 -f "dashboard/app.py"
  rm -f *.pid
  ```
- **Windows PowerShell:**
  ```powershell
  Stop-Process -Name edge_gateway, mcu_simulator -Force -ErrorAction SilentlyContinue
  Remove-Item *.pid -Force -ErrorAction SilentlyContinue
  ```

---

## 3. MQTT Broker Unavailable

### Symptom
Gateway log reports:
`WARN MQTT broker not available at startup. Telemetry will retry in background.`

### Explanation & Resolution
This is **intended behavior**. The gateway is engineered with graceful degradation:
- If Mosquitto is installed and running on port 1883, telemetry is published automatically.
- If no broker exists, the core simulation, Modbus server, control loop, and CLI monitor continue operating without interruption.
- To install Mosquitto:
  - Ubuntu/Debian: `sudo apt install mosquitto mosquitto-clients && sudo systemctl start mosquitto`
  - macOS: `brew install mosquitto && brew services start mosquitto`
  - Windows: Download from `https://mosquitto.org/download/`

---

## 4. Modbus Port 502 vs 1502 Permission Denied

### Symptom
`Failed to bind Modbus TCP server on 0.0.0.0:502`

### Explanation
Ports below 1024 are privileged root ports on Linux. The default configuration uses **Port 1502**, which is standard unprivileged Modbus TCP. If you need standard port 502:
```bash
sudo setcap 'cap_net_bind_service=+ep' ./build/bin/edge_gateway
```
Then set `modbus_port=502` in `config/gateway.conf`.

---

## 5. Wireshark Packet Capture Troubleshooting

### Windows Loopback Capture
Windows does not natively capture loopback traffic using standard WinPcap. 
- Ensure **Npcap** is installed with the option *"Support loopback traffic ("Npcap Loopback Adapter")"* enabled.
- Select the `Npcap Loopback Adapter` interface in Wireshark.

### Linux Loopback Capture
In Wireshark or `tshark`, select the `lo` (Local Loopback) interface:
```bash
tshark -i lo -f "tcp port 9000 or tcp port 1502"
```
