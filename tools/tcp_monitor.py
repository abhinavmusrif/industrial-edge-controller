#!/usr/bin/env python3
"""
CLI TCP Monitoring Tool for Industrial Edge Controller
Connects to Gateway TCP port 9100 to query telemetry, diagnostics, and control.
"""

import socket
import sys
import argparse
import time

def send_command(host, port, cmd):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3.0)
        s.connect((host, port))
        
        # Read initial banner
        initial = s.recv(1024).decode('utf-8', errors='ignore')
        
        # Send command
        s.sendall((cmd + "\r\n").encode('utf-8'))
        
        # Read response
        time.sleep(0.1)
        response = ""
        while True:
            try:
                data = s.recv(2048)
                if not data:
                    break
                response += data.decode('utf-8', errors='ignore')
                if "> " in response:
                    break
            except socket.timeout:
                break
                
        # Send QUIT
        try:
            s.sendall(b"QUIT\r\n")
            s.close()
        except:
            pass
            
        return response
    except Exception as e:
        return f"[ERROR] Failed to connect to TCP monitor at {host}:{port}: {e}"

def interactive_session(host, port):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
        print(f"Connected to Industrial Edge Gateway Monitor at {host}:{port}")
        
        # Print welcome banner
        banner = s.recv(1024).decode('utf-8', errors='ignore')
        print(banner, end="")
        
        while True:
            try:
                cmd = input()
                if not cmd.strip():
                    continue
                s.sendall((cmd + "\r\n").encode('utf-8'))
                if cmd.strip().upper() in ["QUIT", "EXIT"]:
                    break
                
                time.sleep(0.05)
                resp = s.recv(4096).decode('utf-8', errors='ignore')
                print(resp, end="", flush=True)
            except (KeyboardInterrupt, EOFError):
                break
        s.close()
    except Exception as e:
        print(f"[ERROR] Session error: {e}")

def main():
    parser = argparse.ArgumentParser(description="Industrial Edge Gateway TCP CLI Monitor")
    parser.add_argument("--host", default="127.0.0.1", help="Gateway monitor host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=9100, help="Gateway monitor port (default: 9100)")
    parser.add_argument("--cmd", help="Single command to run (e.g. STATUS, SENSORS, FAULTS, JSON)")
    parser.add_argument("-i", "--interactive", action="store_true", help="Launch interactive terminal session")

    args = parser.parse_args()

    if args.interactive or not args.cmd:
        interactive_session(args.host, args.port)
    else:
        out = send_command(args.host, args.port, args.cmd)
        print(out)

if __name__ == "__main__":
    main()
