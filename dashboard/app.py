#!/usr/bin/env python3
"""
Industrial Edge Controller Web Dashboard Server
Runs an HTTP server that queries live Gateway telemetry over TCP port 9100
and serves an interactive SCADA digital twin web dashboard.
"""

import sys
import os
import json
import socket
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATES_DIR = os.path.join(BASE_DIR, "templates")
STATIC_DIR = os.path.join(BASE_DIR, "static")

GATEWAY_HOST = "127.0.0.1"
GATEWAY_PORT = 9100
MCU_FAULT_HOST = "127.0.0.1"
MCU_FAULT_PORT = 9001

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

        # Extract JSON substring
        start = resp.find("{")
        end = resp.rfind("}")
        if start != -1 and end != -1:
            json_str = resp[start:end+1]
            return json.loads(json_str)
    except Exception as e:
        return {
            "device": "device01",
            "timestamp": "OFFLINE",
            "temperature": 0.0,
            "current": 0.0,
            "vibration": 0.0,
            "rpm": 0.0,
            "state": "OFFLINE",
            "mcu_connected": False,
            "watchdog": "OFFLINE",
            "watchdog_elapsed_ms": 0,
            "last_trip_reason": f"Gateway unreachable: {e}",
            "faults": ["Gateway server offline"]
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
                content_type = "text/css" if file_path.endswith(".css") else "application/javascript"
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

        else:
            self.send_error(404, "Not Found")

    def do_POST(self):
        content_len = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_len).decode("utf-8") if content_len > 0 else "{}"
        
        try:
            req_data = json.loads(body)
        except:
            req_data = {}

        if self.path == "/api/actuator":
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
        pass # Suppress HTTP access logging in console

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
