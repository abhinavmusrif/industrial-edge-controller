#!/usr/bin/env python3
"""
Fault Injection Tool for Industrial Edge Controller
Sends dynamic fault triggers to the running MCU simulator and queries
the resulting safety reactions from the Linux Edge Gateway.
"""

import socket
import sys
import argparse
import time

def inject_fault(mcu_host, mcu_port, command):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((mcu_host, mcu_port))
        s.sendall((command + "\n").encode('utf-8'))
        resp = s.recv(1024).decode('utf-8', errors='ignore')
        s.close()
        return resp.strip()
    except Exception as e:
        return f"[ERROR] Failed to send fault command to MCU at {mcu_host}:{mcu_port}: {e}"

def query_gateway_status(gw_host, gw_port):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect((gw_host, gw_port))
        s.recv(1024) # Skip banner
        s.sendall(b"STATUS\r\n")
        time.sleep(0.1)
        resp = s.recv(2048).decode('utf-8', errors='ignore')
        s.sendall(b"QUIT\r\n")
        s.close()
        return resp
    except Exception as e:
        return f"[WARNING] Could not query Gateway monitor: {e}"

def main():
    parser = argparse.ArgumentParser(description="Industrial Edge Controller Fault Injector")
    parser.add_argument("--mcu-host", default="127.0.0.1", help="MCU fault server host (default: 127.0.0.1)")
    parser.add_argument("--mcu-port", type=int, default=9001, help="MCU fault server port (default: 9001)")
    parser.add_argument("--gw-host", default="127.0.0.1", help="Gateway monitor host (default: 127.0.0.1)")
    parser.add_argument("--gw-port", type=int, default=9100, help="Gateway monitor port (default: 9100)")

    parser.add_argument("--temperature-high", action="store_true", help="Inject over-temperature condition (92.5°C > 85°C)")
    parser.add_argument("--current-high", action="store_true", help="Inject over-current condition (16.8A > 15A)")
    parser.add_argument("--vibration-high", action="store_true", help="Inject excessive vibration (8.4 mm/s > 7 mm/s)")
    parser.add_argument("--bad-crc", action="store_true", help="Corrupt CRC-32 checksums on transmitted frames")
    parser.add_argument("--packet-loss", action="store_true", help="Simulate 50% random packet drops")
    parser.add_argument("--network-delay", action="store_true", help="Simulate 300ms transport latency")
    parser.add_argument("--sensor-timeout", action="store_true", help="Halt sensor transmissions to trigger watchdog")
    parser.add_argument("--mcu-disconnect", action="store_true", help="Simulate MCU disconnection / transport drop")
    parser.add_argument("--clear", "--reset", action="store_true", dest="reset", help="Clear all faults and return to nominal")

    args = parser.parse_args()

    cmd = None
    desc = ""
    if args.temperature_high:
        cmd = "TEMP_HIGH"
        desc = "Injecting Over-Temperature Fault (Forced 92.5°C)"
    elif args.current_high:
        cmd = "CURRENT_HIGH"
        desc = "Injecting Over-Current Fault (Forced 16.8A)"
    elif args.vibration_high:
        cmd = "VIBE_HIGH"
        desc = "Injecting High Vibration Fault (Forced 8.4 mm/s)"
    elif args.bad_crc:
        cmd = "BAD_CRC"
        desc = "Injecting Corrupted CRC-32 Frame Checksums"
    elif args.packet_loss:
        cmd = "DROP"
        desc = "Injecting 50% Packet Loss Simulation"
    elif args.network_delay:
        cmd = "DELAY"
        desc = "Injecting 300ms Transport Network Delay"
    elif args.sensor_timeout or args.mcu_disconnect:
        cmd = "PAUSE"
        desc = "Halting MCU Transmissions (Watchdog Timeout Trigger)"
    elif args.reset:
        cmd = "RESET"
        desc = "Clearing all injected faults"
    else:
        parser.print_help()
        sys.exit(1)

    print(f"\n[FAULT INJECTOR] {desc}")
    print(f"[FAULT INJECTOR] Target: MCU @ {args.mcu_host}:{args.mcu_port}")
    
    resp = inject_fault(args.mcu_host, args.mcu_port, cmd)
    print(f"[MCU RESPONSE]   {resp}")

    # Wait for gateway control loop & watchdog to process
    print("[WAITING] Allowing Linux Gateway to detect fault and execute safety action...")
    time.sleep(1.0)

    # Query gateway response
    print("\n--- LINUX GATEWAY SAFETY RESPONSE ---")
    gw_status = query_gateway_status(args.gw_host, args.gw_port)
    print(gw_status)

if __name__ == "__main__":
    main()
