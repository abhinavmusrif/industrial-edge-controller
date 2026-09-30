#!/usr/bin/env python3
"""
Modbus TCP Client for Industrial Edge Controller
Demonstrates low-level industrial SCADA integration by directly building
MBAP headers and querying holding registers 40001-40008 over standard sockets.
"""

import socket
import struct
import sys
import argparse
import time

STATE_NAMES = {
    0: "OFF",
    1: "STARTING",
    2: "RUNNING",
    3: "WARNING",
    4: "EMERGENCY_STOP",
    5: "SAFE_STOP"
}

FAULT_NAMES = {
    0: "NONE",
    1: "HIGH_TEMPERATURE",
    2: "HIGH_CURRENT",
    3: "HIGH_VIBRATION",
    4: "SENSOR_STALE",
    5: "WATCHDOG_TIMEOUT",
    6: "CRC_ERROR",
    7: "MCU_DISCONNECTED"
}

def read_holding_registers(host, port, unit_id, start_addr, count):
    """
    Constructs and sends Modbus TCP ADU:
    MBAP Header (7 bytes):
      Transaction ID: 2 bytes (0x0001)
      Protocol ID:    2 bytes (0x0000 = Modbus)
      Length:         2 bytes (0x0006 = 6 bytes follow)
      Unit ID:        1 byte
    PDU:
      Function Code:  1 byte (0x03 = Read Holding Registers)
      Start Address:  2 bytes
      Register Count: 2 bytes
    """
    trans_id = 1
    proto_id = 0
    length = 6
    func_code = 3

    request = struct.pack(">HHHBBHH", trans_id, proto_id, length, unit_id, func_code, start_addr, count)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(3.0)
    s.connect((host, port))
    s.sendall(request)

    response = s.recv(260)
    s.close()

    if len(response) < 9:
        raise ValueError(f"Invalid response length: {len(response)} bytes")

    resp_trans, resp_proto, resp_len, resp_unit, resp_func, byte_count = struct.unpack(">HHHBBB", response[:9])
    
    if resp_func != 0x03:
        raise ValueError(f"Modbus exception returned: function code 0x{resp_func:02X}")

    reg_data = response[9:9 + byte_count]
    num_regs = byte_count // 2
    registers = struct.unpack(f">{num_regs}H", reg_data)
    return registers

def display_scada_table(registers):
    if len(registers) < 8:
        print("[ERROR] Expected at least 8 registers")
        return

    temp = registers[0] / 10.0
    curr = registers[1] / 10.0
    vibe = registers[2] / 10.0
    rpm  = registers[3]
    state_code = registers[4]
    fault_code = registers[5]
    watchdog = "OK" if registers[6] == 1 else "EXPIRED/FAULT"
    mcu_conn = "CONNECTED" if registers[7] == 1 else "DISCONNECTED"

    state_str = STATE_NAMES.get(state_code, f"UNKNOWN({state_code})")
    fault_str = FAULT_NAMES.get(fault_code, f"CODE({fault_code})")

    print("\n" + "=" * 65)
    print(" MODBUS TCP SCADA REGISTER MAP (Port 1502)")
    print("=" * 65)
    print(f" {'REG':<8} {'ADDR':<8} {'PARAMETER':<20} {'RAW':<8} {'SCALED VALUE'}")
    print("-" * 65)
    print(f" 40001    0x0000   Temperature          {registers[0]:<8} {temp:.1f} °C")
    print(f" 40002    0x0001   Motor Current        {registers[1]:<8} {curr:.1f} A")
    print(f" 40003    0x0002   Vibration            {registers[2]:<8} {vibe:.1f} mm/s")
    print(f" 40004    0x0003   Motor Speed          {registers[3]:<8} {rpm} RPM")
    print(f" 40005    0x0004   Actuator State       {registers[4]:<8} {state_str}")
    print(f" 40006    0x0005   Fault Code           {registers[5]:<8} {fault_str}")
    print(f" 40007    0x0006   Watchdog Status      {registers[6]:<8} {watchdog}")
    print(f" 40008    0x0007   MCU Link Status      {registers[7]:<8} {mcu_conn}")
    print("=" * 65)

def main():
    parser = argparse.ArgumentParser(description="Modbus TCP SCADA Client")
    parser.add_argument("--host", default="127.0.0.1", help="Modbus server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=1502, help="Modbus server port (default: 1502)")
    parser.add_argument("--unit", type=int, default=1, help="Modbus Unit ID (default: 1)")
    parser.add_argument("--loop", action="store_true", help="Poll continuously every second")

    args = parser.parse_args()

    while True:
        try:
            regs = read_holding_registers(args.host, args.port, args.unit, 0, 8)
            display_scada_table(regs)
        except Exception as e:
            print(f"[ERROR] Modbus query failed: {e}")

        if not args.loop:
            break
        time.sleep(1.0)

if __name__ == "__main__":
    main()
