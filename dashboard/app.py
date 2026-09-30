#!/usr/bin/env python3
"""
Industrial Edge Controller Web Dashboard Server
Runs an HTTP server that queries live Gateway telemetry over TCP port 9100,
serves an interactive SCADA digital twin web dashboard, and embeds a virtual Linux shell.
"""

import sys
import os
import json
import socket
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler

# Import Linux Shell Engine
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from linux_shell import LinuxShellEngine

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "templates")
STATIC_DIR = os.path.join(BASE_DIR, "static")

GATEWAY_HOST = "127.0.0.1"
GATEWAY_PORT = 9100
MCU_FAULT_HOST = "127.0.0.1"
MCU_FAULT_PORT = 9001

shell_engine = LinuxShellEngine()

def query_gateway_json():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(1.5)
        s.connect((GATEWAY_HOST, GATEWAY_PORT))
        s.recv(1024) # Banner
        s.sendall(b"JSON\r\n")
        
        resp = ""
        while True:
            chunk = s.recv(2048).decode('utf-8', errors='ignore')
            if not chunk:
                break
            resp += chunk
            if "> " in resp:
                break
        s.sendall(b"QUIT\r\n")
        s.close()

        start = resp.find("{")
        end = resp.rfind("}")
        if start != -1 and end != -1:
            json_str = resp[start:end+1]
            return json.loads(json_str)
    except Exception as e:
        return {
            "device": "device01",
            "timestamp": "OFFLINE",
            "temperature": 50.0,
            "current": 6.5,
            "vibration": 1.5,
            "rpm": 1600.0,
            "state": "RUNNING",
            "mcu_connected": True,
            "watchdog": "OK",
            "watchdog_elapsed_ms": 32,
            "last_trip_reason": f"Active",
            "faults": []
        }

def get_hardware_info():
    t = query_gateway_json()
    temp = float(t.get("temperature", 50.0))
    curr = float(t.get("current", 6.5))
    vibe = float(t.get("vibration", 1.5))
    rpm = float(t.get("rpm", 1600.0))
    state = str(t.get("state", "RUNNING"))
    
    return {
        "devices": [
            {
                "name": "TMP117",
                "part": "TMP117AIDRVR",
                "bus": "I2C-1",
                "address": "0x48",
                "desc": "High-precision ±0.1°C digital temperature sensor with 16-bit ADC",
                "registers": [
                    {"addr": "0x00", "name": "TEMP_RESULT", "val": f"0x{int(temp / 0.0078125):04X}", "scaled": f"{temp:.2f} °C", "ro": True},
                    {"addr": "0x01", "name": "CONFIG", "val": "0x0220", "scaled": "Continuous 16-avg", "ro": False},
                    {"addr": "0x02", "name": "THIGH_LIMIT", "val": "0x2A80", "scaled": "85.00 °C", "ro": False},
                    {"addr": "0x03", "name": "TLOW_LIMIT", "val": "0x1180", "scaled": "35.00 °C", "ro": False},
                ]
            },
            {
                "name": "INA219",
                "part": "INA219BIDR",
                "bus": "I2C-1",
                "address": "0x40",
                "desc": "Bi-directional current shunt & 32V power monitor",
                "registers": [
                    {"addr": "0x00", "name": "CONFIG", "val": "0x399F", "scaled": "32V FSR, ±320mV", "ro": False},
                    {"addr": "0x01", "name": "SHUNT_VOLTAGE", "val": f"0x{int(curr * 10):04X}", "scaled": f"{curr*10:.1f} mV", "ro": True},
                    {"addr": "0x02", "name": "BUS_VOLTAGE", "val": "0x1770", "scaled": "24.00 V", "ro": True},
                    {"addr": "0x04", "name": "CURRENT_RAW", "val": f"0x{int(curr / 0.01):04X}", "scaled": f"{curr:.2f} A", "ro": True},
                ]
            },
            {
                "name": "MPU-6050",
                "part": "MPU-6050",
                "bus": "I2C-1",
                "address": "0x68",
                "desc": "Triple-axis MEMS accelerometer with digital motion processor",
                "registers": [
                    {"addr": "0x3B", "name": "ACCEL_XOUT", "val": "0x0080", "scaled": "0.01 g", "ro": True},
                    {"addr": "0x3D", "name": "ACCEL_YOUT", "val": "0x0060", "scaled": "0.01 g", "ro": True},
                    {"addr": "0x3F", "name": "ACCEL_ZOUT", "val": "0x4000", "scaled": "1.00 g (Gravity)", "ro": True},
                    {"addr": "0x41", "name": "VIBE_RMS", "val": f"0x{int(vibe / 0.01):04X}", "scaled": f"{vibe:.2f} mm/s", "ro": True},
                    {"addr": "0x75", "name": "WHO_AM_I", "val": "0x0068", "scaled": "Device ID Verified", "ro": True},
                ]
            },
            {
                "name": "ENCODER",
                "part": "TIM2_ENCODER",
                "bus": "TIM-2",
                "address": "0x00",
                "desc": "Hardware 32-bit quadrature timer counter coupled to motor shaft",
                "registers": [
                    {"addr": "0x00", "name": "TIM_CNT", "val": "0x4E20", "scaled": "20000 pulses", "ro": True},
                    {"addr": "0x04", "name": "SPEED_RPM", "val": f"0x{int(rpm):04X}", "scaled": f"{rpm:.0f} RPM", "ro": True},
                    {"addr": "0x08", "name": "TIM_ARR", "val": "0xFFFF", "scaled": "65535 max modulus", "ro": False},
                ]
            }
        ],
        "gpio": [
            {"pin": 0, "name": "MOTOR_PWM_ENABLE", "dir": "OUT", "val": 1 if state in ["RUNNING", "STARTING", "WARNING"] else 0, "desc": "H-Bridge Gate Drive Enable (Active HIGH)"},
            {"pin": 1, "name": "ESTOP_RELAY_TRIP", "dir": "OUT", "val": 1 if state in ["EMERGENCY_STOP", "SAFE_STOP"] else 0, "desc": "Emergency Stop Contactor Relay (0=Normal Closed, 1=Open/Trip)"},
            {"pin": 2, "name": "WATCHDOG_WDI", "dir": "OUT", "val": 1 if t.get("watchdog_ok", True) else 0, "desc": "External Watchdog Heartbeat Input Line (Toggling)"},
            {"pin": 3, "name": "OPERATOR_RESET_PB", "dir": "IN", "val": 0, "desc": "Physical Cabinet Push Button (Momentary Active HIGH)"},
            {"pin": 4, "name": "WARNING_BEACON_LED", "dir": "OUT", "val": 1 if state in ["WARNING", "EMERGENCY_STOP", "SAFE_STOP"] else 0, "desc": "Yellow Warning Tower Beacon (0=OFF, 1=ON)"},
        ]
    }

def send_gateway_command(cmd):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((GATEWAY_HOST, GATEWAY_PORT))
        s.recv(1024)
        s.sendall((cmd + "\r\n").encode('utf-8'))
        resp = s.recv(1024).decode('utf-8', errors='ignore')
        s.sendall(b"QUIT\r\n")
        s.close()
        return resp
    except Exception as e:
        return str(e)

def send_mcu_fault(cmd):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((MCU_FAULT_HOST, MCU_FAULT_PORT))
        s.sendall((cmd + "\n").encode('utf-8'))
        resp = s.recv(1024).decode('utf-8', errors='ignore')
        s.close()
        return resp
    except Exception as e:
        return str(e)

class DashboardRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            index_path = os.path.join(TEMPLATES_DIR, "index.html")
            with open(index_path, "rb") as f:
                content = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        elif self.path.startswith("/static/"):
            rel_file = self.path[len("/static/"):]
            file_path = os.path.join(STATIC_DIR, rel_file)
            if os.path.exists(file_path):
                content_type = "text/css" if file_path.endswith(".css") else \
                               "application/javascript" if file_path.endswith(".js") else \
                               "image/jpeg" if file_path.endswith(".jpg") else "application/octet-stream"
                with open(file_path, "rb") as f:
                    content = f.read()
                self.send_response(200)
                self.send_header("Content-Type", content_type)
                self.send_header("Content-Length", str(len(content)))
                self.end_headers()
                self.wfile.write(content)
            else:
                self.send_error(404, "File Not Found")

        elif self.path == "/api/telemetry":
            data = query_gateway_json()
            payload = json.dumps(data).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        elif self.path == "/api/hardware":
            data = get_hardware_info()
            payload = json.dumps(data).encode('utf-8')
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        else:
            self.send_error(404, "Not Found")

    def do_POST(self):
        content_len = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_len).decode("utf-8") if content_len > 0 else "{}"
        
        try:
            req_data = json.loads(body)
        except:
            req_data = {}

        if self.path == "/api/terminal":
            cmd_line = req_data.get("command", "")
            output = shell_engine.execute(cmd_line)
            out = json.dumps({
                "output": output,
                "cwd": shell_engine.cwd,
                "prompt": f"root@industrial-edge:{shell_engine.cwd}# "
            }).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(out)))
            self.end_headers()
            self.wfile.write(out)

        elif self.path == "/api/actuator":
            cmd = req_data.get("command", "")
            resp = send_gateway_command(cmd)
            out = json.dumps({"status": "ok", "response": resp}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(out)))
            self.end_headers()
            self.wfile.write(out)

        elif self.path == "/api/fault":
            fault_cmd = req_data.get("fault", "")
            resp = send_mcu_fault(fault_cmd)
            out = json.dumps({"status": "ok", "response": resp}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(out)))
            self.end_headers()
            self.wfile.write(out)

        else:
            self.send_error(404, "Not Found")

    def log_message(self, format, *args):
        pass

def main():
    parser = argparse.ArgumentParser(description="Industrial Edge Dashboard Server")
    parser.add_argument("--port", type=int, default=8080, help="Web dashboard HTTP port (default: 8080)")
    parser.add_argument("--gw-port", type=int, default=9100, help="Gateway monitor port (default: 9100)")
    parser.add_argument("--fault-port", type=int, default=9001, help="MCU fault port (default: 9001)")

    args = parser.parse_args()
    global GATEWAY_PORT, MCU_FAULT_PORT
    GATEWAY_PORT = args.gw_port
    MCU_FAULT_PORT = args.fault_port

    server_address = ('0.0.0.0', args.port)
    httpd = HTTPServer(server_address, DashboardRequestHandler)
    print(f"\n=======================================================")
    print(f" Digital Twin Monitoring Dashboard Active")
    print(f" Access URL: http://localhost:{args.port}")
    print(f"=======================================================\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping Dashboard...")
        httpd.server_close()

if __name__ == "__main__":
    main()
