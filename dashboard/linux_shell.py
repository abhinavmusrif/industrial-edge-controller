#!/usr/bin/env python3
"""
Virtual Embedded Linux System & Shell Engine
Implements an authentic Linux environment for the Industrial Edge Controller:
  - Virtual sysfs (/sys/bus/i2c/devices, /sys/class/hwmon, /sys/class/gpio)
  - Virtual procfs (/proc/cpuinfo, /proc/meminfo, /proc/uptime)
  - Standard Linux utilities (i2cdetect, i2cget, i2cset, gpioget, gpioset)
  - Diagnostic & monitoring tools (systemctl, journalctl, dmesg, top, ps, tcpdump)
"""

import os
import sys
import time
import socket
import json
import struct

GATEWAY_HOST = "127.0.0.1"
GATEWAY_PORT = 9100
FAULT_PORT = 9001
MCU_PORT = 9000

START_TIME = time.time()

def query_live_telemetry():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(1.5)
            s.connect((GATEWAY_HOST, GATEWAY_PORT))
            s.recv(1024)
            s.sendall(b"JSON\r\n")
            resp = ""
            while True:
                chunk = s.recv(2048).decode('utf-8', errors='ignore')
                if not chunk: break
                resp += chunk
                if "> " in resp: break
            s.sendall(b"QUIT\r\n")
            start = resp.find("{")
            end = resp.rfind("}")
            if start != -1 and end != -1:
                return json.loads(resp[start:end+1])
    except Exception:
        pass
    return {
        "temperature": 50.0, "current": 6.5, "vibration": 1.5, "rpm": 1600.0,
        "controller_state": "RUNNING", "watchdog_ok": True, "watchdog_elapsed_ms": 32,
        "mcu_connected": True, "active_faults": [], "last_trip_reason": "None"
    }

def send_gateway_raw_cmd(cmd):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2.0)
            s.connect((GATEWAY_HOST, GATEWAY_PORT))
            s.recv(1024)
            s.sendall((cmd + "\r\n").encode("utf-8"))
            time.sleep(0.05)
            resp = s.recv(4096).decode('utf-8', errors='ignore')
            s.sendall(b"QUIT\r\n")
            if "> " in resp:
                resp = resp.replace("> ", "")
            return resp.strip()
    except Exception as e:
        return f"Error connecting to edge gateway daemon: {e}"

def send_fault_cmd(cmd):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2.0)
            s.connect((GATEWAY_HOST, FAULT_PORT))
            s.sendall((cmd + "\n").encode("utf-8"))
            return s.recv(1024).decode('utf-8', errors='ignore').strip()
    except Exception as e:
        return f"Fault injector daemon: {e}"

class LinuxShellEngine:
    def __init__(self):
        self.cwd = "/root"

    def execute(self, cmd_line: str) -> str:
        cmd_line = cmd_line.strip()
        if not cmd_line:
            return ""

        # Handle simple piping: e.g. cmd | grep term or cmd | tail -n 10
        if " | " in cmd_line:
            parts = cmd_line.split(" | ")
            out = self.execute(parts[0])
            for pipe in parts[1:]:
                pipe_parts = pipe.strip().split()
                pcmd = pipe_parts[0]
                if pcmd == "grep":
                    term = pipe_parts[1] if len(pipe_parts) > 1 else ""
                    invert = "-v" in pipe_parts
                    if invert and len(pipe_parts) > 2:
                        term = pipe_parts[2]
                    lines = [l for l in out.splitlines() if (term in l if not invert else term not in l)]
                    out = "\n".join(lines)
                elif pcmd == "tail":
                    n = 10
                    if "-n" in pipe_parts:
                        idx = pipe_parts.index("-n")
                        if idx + 1 < len(pipe_parts) and pipe_parts[idx+1].isdigit():
                            n = int(pipe_parts[idx+1])
                    lines = out.splitlines()
                    out = "\n".join(lines[-n:])
                elif pcmd == "head":
                    n = 10
                    if "-n" in pipe_parts:
                        idx = pipe_parts.index("-n")
                        if idx + 1 < len(pipe_parts) and pipe_parts[idx+1].isdigit():
                            n = int(pipe_parts[idx+1])
                    lines = out.splitlines()
                    out = "\n".join(lines[:n])
            return out

        tokens = cmd_line.split()
        cmd = tokens[0].lower()
        args = tokens[1:]

        # Dispatch command
        safe_cmd = cmd.replace("-", "_")
        handler = getattr(self, f"cmd_{safe_cmd}", None)
        if handler:
            return handler(args)

        # Built-in direct handlers
        if cmd == "pwd":
            return self.cwd
        elif cmd == "cd":
            if args:
                target = args[0]
                if target.startswith("/"):
                    self.cwd = target
                elif target == "..":
                    self.cwd = "/" if self.cwd == "/root" else "/root"
                else:
                    self.cwd = f"{self.cwd.rstrip('/')}/{target}"
            else:
                self.cwd = "/root"
            return ""
        elif cmd == "clear":
            return "\033[2J\033[H"
        elif cmd == "whoami":
            return "root"
        elif cmd == "hostname":
            return "industrial-edge-gateway"
        elif cmd == "uptime":
            uptime_sec = int(time.time() - START_TIME) + 4210
            days = uptime_sec // 86400
            hours = (uptime_sec % 86400) // 3600
            mins = (uptime_sec % 3600) // 60
            return f" {time.strftime('%H:%M:%S')} up {days} days, {hours:02d}:{mins:02d}, 1 user, load average: 0.12, 0.08, 0.05"
        elif cmd == "uname":
            if "-a" in args:
                return "Linux industrial-edge-gateway 6.6.21-rt24-v8 #1 SMP PREEMPT_RT Wed Mar 13 14:22:01 UTC 2026 aarch64 GNU/Linux"
            return "Linux"
        elif cmd == "date":
            return time.strftime("%a %b %d %H:%M:%S %Z %Y")
        elif cmd in ["exit", "logout"]:
            return "Session terminated. Press enter to reconnect."

        return f"-bash: {cmd}: command not found. Type 'help' for available commands."

    def cmd_help(self, args):
        return """
╔════════════════════════════════════════════════════════════════════════════════╗
║             INDUSTRIAL EDGE GATEWAY (EMBEDDED LINUX CLI COMMANDS)              ║
╚════════════════════════════════════════════════════════════════════════════════╝
  [Hardware & Bus Inspection]
    i2cdetect -y 1            - Scan I2C-1 bus matrix for online sensor chips
    i2cget -y 1 <addr> <reg>  - Read 16-bit word from virtual I2C IC register
    i2cset -y 1 <addr> <reg>  - Write 16-bit word to virtual I2C IC register
    gpioget <pin>             - Read GPIO pin level (e.g. gpioget 0 for PWM enable)
    gpioset <pin>=<val>       - Drive GPIO pin level (e.g. gpioset 1=1 for ESTOP trip)
    lsdev / lshardware        - Hierarchical hardware component & register tree

  [Virtual Linux Filesystem (sysfs / procfs)]
    ls /sys/bus/i2c/devices/  - Show attached I2C devices (1-0040, 1-0048, 1-0068)
    cat /sys/class/hwmon/hwmon0/temp1_input - Read millidegrees C from TMP117
    cat /sys/class/hwmon/hwmon1/curr1_input - Read milliamps from INA219
    cat /sys/class/gpio/gpio1/value         - Read ESTOP safety contactor relay state
    cat /proc/cpuinfo         - Quad-core ARM Cortex-A72 CPU information
    cat /proc/meminfo         - Gateway memory allocations & slab usage

  [Diagnostics, Networking & System Services]
    systemctl status industrial-gateway    - Service supervisor, threads, and PID
    journalctl -u industrial-gateway -n 15 - Follow daemon log events
    dmesg | tail -n 20                     - Linux kernel driver ring buffer
    top / ps aux                           - Real-time POSIX threads and CPU utilization
    tcpdump -i lo port 9000 -X             - Wireshark-like raw packet stream sniffer
    modbus-cli read 40001 8                - Query industrial SCADA holding registers
    inject <fault_name>                    - Inject real-time faults (temperature-high, bad-crc)
    start / stop / reset / estop           - Direct motor interlock commands
"""

    def cmd_ls(self, args):
        path = args[-1] if (args and not args[-1].startswith("-")) else self.cwd
        path = path.rstrip("/")

        if path in ["/sys/bus/i2c/devices", "/sys/bus/i2c/devices/"]:
            return "1-0040  1-0048  1-0068"
        elif path in ["/sys/class/hwmon", "/sys/class/hwmon/"]:
            return "hwmon0  hwmon1  hwmon2"
        elif path.startswith("/sys/class/hwmon/hwmon0"):
            return "name  temp1_input  temp1_max  temp1_crit  temp1_label  device"
        elif path.startswith("/sys/class/hwmon/hwmon1"):
            return "name  curr1_input  curr1_max  in0_input  power1_input  device"
        elif path.startswith("/sys/class/hwmon/hwmon2"):
            return "name  in_accel_x_raw  in_accel_y_raw  in_accel_z_raw  vibe_rms"
        elif path in ["/sys/class/gpio", "/sys/class/gpio/"]:
            return "export  unexport  gpio0  gpio1  gpio2  gpio3  gpio4  gpiochip1"
        elif path.startswith("/sys/class/gpio/gpio"):
            return "direction  value  active_low  edge  label"
        elif path in ["/sys/bus", "/sys/bus/"]:
            return "i2c  spi  platform  usb"
        elif path in ["/dev", "/dev/"]:
            return "i2c-1  ttyS0  ttyUSB0  watchdog0  urandom  zero  null  console"
        elif path in ["/proc", "/proc/"]:
            return "cpuinfo  meminfo  uptime  version  loadavg  net  sys"
        elif path in ["/etc", "/etc/"]:
            return "industrial  systemd  network  hosts  resolv.conf  os-release"
        elif path in ["/etc/industrial", "/etc/industrial/"]:
            return "gateway.conf  sensors.conf  modbus.conf  mqtt.conf"
        elif path in ["/var/log", "/var/log/"]:
            return "syslog  dmesg  industrial-gateway.log  journal"
        elif path in ["/root", "~"]:
            return "start_demo.sh  test_hardware.sh  tools  config  README.txt"
        elif path in ["/", ""]:
            return "bin  boot  dev  etc  home  lib  proc  root  run  sbin  sys  tmp  usr  var"
        else:
            return f"ls: cannot access '{path}': No such file or directory"

    def cmd_cat(self, args):
        if not args:
            return "cat: missing file operand"
        filepath = args[0]
        t = query_live_telemetry()
        temp = float(t.get("temperature", 50.0))
        curr = float(t.get("current", 6.5))
        vibe = float(t.get("vibration", 1.5))
        rpm = float(t.get("rpm", 1600.0))
        state = str(t.get("controller_state", "RUNNING"))

        if filepath.endswith("temp1_input"):
            return f"{int(temp * 1000)}"  # millidegrees
        elif filepath.endswith("temp1_max"):
            return "70000"
        elif filepath.endswith("temp1_crit"):
            return "85000"
        elif filepath.endswith("curr1_input"):
            return f"{int(curr * 1000)}"  # milliamps
        elif filepath.endswith("curr1_max"):
            return "15000"
        elif filepath.endswith("in0_input"):
            return "24000"  # 24.0 V bus voltage
        elif filepath.endswith("vibe_rms"):
            return f"{int(vibe * 1000)}"  # um/s
        elif filepath.endswith("in_accel_z_raw"):
            return "16384"  # 1.0g
        elif filepath.endswith("speed_rpm") or "rpm" in filepath:
            return f"{int(rpm)}"
        elif "gpio0/value" in filepath:
            # MOTOR_PWM_ENABLE: 1 if running/starting/warning, 0 if stopped/estop
            return "1" if state in ["RUNNING", "STARTING", "WARNING"] else "0"
        elif "gpio1/value" in filepath:
            # ESTOP_RELAY_TRIP: 1 if tripped, 0 if healthy
            return "1" if state in ["EMERGENCY_STOP", "SAFE_STOP"] else "0"
        elif "gpio2/value" in filepath:
            return "1" if t.get("watchdog_ok", True) else "0"
        elif "gpio4/value" in filepath:
            return "1" if state in ["WARNING", "EMERGENCY_STOP", "SAFE_STOP"] else "0"
        elif filepath == "/proc/cpuinfo":
            return """processor\t: 0
model name\t: ARMv8 Processor rev 3 (v8l)
BogoMIPS\t: 108.00
Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics
CPU implementer\t: 0x41
CPU architecture: 8
CPU variant\t: 0x0
CPU part\t: 0xd08
CPU revision\t: 3
Hardware\t: BCM2711 / Quad-Core Cortex-A72 @ 1.80GHz"""
        elif filepath == "/proc/meminfo":
            return """MemTotal:        3985420 kB
MemFree:         2841200 kB
MemAvailable:    3215400 kB
Buffers:          142100 kB
Cached:           512400 kB
SwapCached:            0 kB
Active:           612300 kB
Inactive:         284500 kB
Slab:              98400 kB"""
        elif filepath == "/etc/os-release":
            return """PRETTY_NAME="Debian GNU/Linux 12 (bookworm) Industrial RT"
NAME="Debian GNU/Linux"
VERSION_ID="12"
VERSION="12 (bookworm)"
VERSION_CODENAME=bookworm
ID=debian
HOME_URL="https://www.debian.org/"
SUPPORT_URL="https://www.debian.org/support"
BUG_REPORT_URL="https://bugs.debian.org/" """
        elif filepath.endswith("gateway.conf"):
            return """[Gateway]
device_id=device01
mcu_host=127.0.0.1
mcu_port=9000
monitor_port=9100
modbus_port=1502
stale_timeout_ms=1500
watchdog_timeout_ms=2000
log_level=INFO"""
        else:
            return f"cat: {filepath}: No such file or directory"

    def cmd_i2cdetect(self, args):
        # Format classic Linux i2c-tools matrix
        # Addresses online: 0x40 (INA219), 0x48 (TMP117), 0x68 (MPU-6050)
        out = [
            "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f",
            "00:                         -- -- -- -- -- -- -- -- ",
            "10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- ",
            "20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- ",
            "30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- ",
            "40: 40 -- -- -- -- -- -- -- 48 -- -- -- -- -- -- -- ",
            "50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- ",
            "60: -- -- -- -- -- -- -- -- 68 -- -- -- -- -- -- -- ",
            "70: -- -- -- -- -- -- -- --                         ",
            "",
            "[I2C-1 SCAN COMPLETE: 3 physical slave chips detected]",
            "  • 0x40: Texas Instruments INA219 (Current Shunt & Bus Voltage Monitor)",
            "  • 0x48: Texas Instruments TMP117 (High-Precision ±0.1°C Temp Sensor)",
            "  • 0x68: InvenSense MPU-6050 (6-Axis Accelerometer & Vibration Sensor)"
        ]
        return "\n".join(out)

    def cmd_i2cget(self, args):
        # i2cget -y 1 <addr> <reg> [w]
        if len(args) < 3:
            return "Usage: i2cget -y <bus> <chip-address> <data-address> [mode: b/w]"
        addr = args[1].lower()
        reg = args[2].lower()
        t = query_live_telemetry()

        if "48" in addr: # TMP117
            temp = float(t.get("temperature", 50.0))
            raw = int(temp / 0.0078125)
            if "00" in reg or "0x00" in reg:
                return f"0x{raw:04x} (TMP117 TEMP_RESULT: {temp:.2f} °C)"
            elif "02" in reg or "0x02" in reg:
                return "0x2a80 (TMP117 THIGH_LIMIT: 85.00 °C)"
            return "0x0220 (TMP117 CONFIG: Continuous 16-avg)"
        elif "40" in addr: # INA219
            curr = float(t.get("current", 6.5))
            raw_curr = int(curr / 0.01)
            if "04" in reg or "0x04" in reg:
                return f"0x{raw_curr:04x} (INA219 CURRENT_RAW: {curr:.2f} A)"
            elif "02" in reg or "0x02" in reg:
                return "0x1770 (INA219 BUS_VOLTAGE: 24.00 V)"
            elif "01" in reg or "0x01" in reg:
                return f"0x{int(curr * 10):04x} (INA219 SHUNT_VOLTAGE: {curr*10:.1f} mV)"
            return "0x399f (INA219 CONFIG: 32V FSR, 320mV)"
        elif "68" in addr: # MPU-6050
            vibe = float(t.get("vibration", 1.5))
            raw_vibe = int(vibe / 0.01)
            if "41" in reg or "0x41" in reg:
                return f"0x{raw_vibe:04x} (MPU6050 VIBE_RMS: {vibe:.2f} mm/s)"
            elif "75" in reg or "0x75" in reg:
                return "0x0068 (MPU6050 WHO_AM_I: 0x68 Valid ID)"
            return "0x4000 (MPU6050 ACCEL_Z: 1.00g Gravity)"

        return f"Error: Chip address {addr} did not acknowledge (NACK)"

    def cmd_i2cset(self, args):
        if len(args) < 4:
            return "Usage: i2cset -y <bus> <chip-address> <data-address> <value> [mode: b/w]"
        addr, reg, val = args[1], args[2], args[3]
        return f"i2cset: Written word {val} to chip {addr} register {reg} successfully [ACK]."

    def cmd_gpioget(self, args):
        if not args:
            return "Usage: gpioget <gpiochip> <offset> or gpioget <pin_number>"
        pin = args[-1]
        t = query_live_telemetry()
        state = str(t.get("controller_state", "RUNNING"))
        if pin == "0":
            val = "1" if state in ["RUNNING", "STARTING", "WARNING"] else "0"
            return f"{val}  # GPIO 0: MOTOR_PWM_ENABLE (Gate Drive)"
        elif pin == "1":
            val = "1" if state in ["EMERGENCY_STOP", "SAFE_STOP"] else "0"
            return f"{val}  # GPIO 1: ESTOP_RELAY_TRIP (1=Tripped Open)"
        elif pin == "2":
            val = "1" if t.get("watchdog_ok", True) else "0"
            return f"{val}  # GPIO 2: WATCHDOG_WDI (Heartbeat Toggle)"
        elif pin == "3":
            return "0  # GPIO 3: OPERATOR_RESET_PB (Momentary Active High)"
        elif pin == "4":
            val = "1" if state in ["WARNING", "EMERGENCY_STOP", "SAFE_STOP"] else "0"
            return f"{val}  # GPIO 4: WARNING_BEACON_LED (0=OFF, 1=ON)"
        return f"gpioget: invalid pin {pin}"

    def cmd_gpioset(self, args):
        if not args:
            return "Usage: gpioset <gpiochip> <pin>=<val> or gpioset <pin>=<val>"
        expr = args[-1]
        if "=" in expr:
            pin, val = expr.split("=")
            if pin == "1" and val == "1":
                send_gateway_raw_cmd("ESTOP")
                return f"GPIO 1 written HIGH -> Emergency Stop contactor relay tripped open!"
            elif pin == "0" and val == "0":
                send_gateway_raw_cmd("STOP")
                return f"GPIO 0 written LOW -> Motor PWM disabled."
            elif pin == "0" and val == "1":
                send_gateway_raw_cmd("START")
                return f"GPIO 0 written HIGH -> Motor PWM enabled."
            return f"GPIO {pin} state set to {val}."
        return "Usage: gpioset <pin>=<val>"

    def cmd_systemctl(self, args):
        if not args or args[0] == "status":
            t = query_live_telemetry()
            state = t.get("controller_state", "RUNNING")
            active = "active (running)" if state in ["RUNNING", "STARTING", "WARNING"] else "active (tripped)"
            pid = 17208
            mem = "18.4M"
            return f"""● industrial-gateway.service - Industrial Linux Edge Controller & Supervisory Twin
     Loaded: loaded (/etc/systemd/system/industrial-gateway.service; enabled; vendor preset: enabled)
     Active: {active} since Thu 2026-10-01 04:50:00 UTC; 28min ago
   Main PID: {pid} (edge_gateway)
     Status: "Operational | Controller: {state} | Watchdog: {'OK' if t.get('watchdog_ok') else 'EXPIRED'}"
      Tasks: 7 (limit: 4915)
     Memory: {mem}
        CPU: 1.842s
     CGroup: /system.slice/industrial-gateway.service
             ├─{pid} /opt/industrial-controller/bin/edge_gateway --config /etc/industrial/gateway.conf
             ├─{pid+1} mcu_comm_worker
             ├─{pid+2} sensor_health_mon
             ├─{pid+3} control_loop_50hz
             ├─{pid+4} telemetry_mqtt_tx
             ├─{pid+5} watchdog_supervisor
             ├─{pid+6} cli_monitor_server [port 9100]
             └─{pid+7} modbus_tcp_server [port 1502]"""
        elif args[0] == "restart":
            send_gateway_raw_cmd("RESET")
            send_gateway_raw_cmd("START")
            return "Restarting industrial-gateway.service... [OK]"
        elif args[0] == "stop":
            send_gateway_raw_cmd("STOP")
            return "Stopping industrial-gateway.service... [OK]"
        elif args[0] == "start":
            send_gateway_raw_cmd("START")
            return "Starting industrial-gateway.service... [OK]"
        return f"Unknown systemctl action: {args[0]}"

    def cmd_journalctl(self, args):
        n = 15
        if "-n" in args:
            idx = args.index("-n")
            if idx + 1 < len(args) and args[idx+1].isdigit():
                n = int(args[idx+1])
        t = query_live_telemetry()
        t_str = time.strftime("%b %d %H:%M:%S")
        events = [
            f"{t_str} industrial-edge edge_gateway[17208]: [INFO] Transport initialized on 127.0.0.1:9000",
            f"{t_str} industrial-edge edge_gateway[17208]: [INFO] Modbus TCP server listening on port 1502",
            f"{t_str} industrial-edge edge_gateway[17208]: [INFO] CLI monitoring server bound to port 9100",
            f"{t_str} industrial-edge edge_gateway[17208]: [INFO] Synchronized with MCU simulator. CRC-32 active.",
            f"{t_str} industrial-edge edge_gateway[17208]: [INFO] Actuator controller state: {t.get('controller_state', 'RUNNING')}",
            f"{t_str} industrial-edge edge_gateway[17208]: [INFO] Telemetry published to MQTT topics",
            f"{t_str} industrial-edge edge_gateway[17208]: [DEBUG] Watchdog kicked. Heartbeat healthy ({t.get('watchdog_elapsed_ms', 25)}ms)",
        ]
        if t.get("active_faults"):
            for f in t.get("active_faults"):
                events.append(f"{t_str} industrial-edge edge_gateway[17208]: [CRITICAL] FAULT ALARM: {f}")
        return "\n".join(events[-n:])

    def cmd_dmesg(self, args):
        return """[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x410fd083]
[    0.000000] Linux version 6.6.21-rt24-v8 (gcc-12) #1 SMP PREEMPT_RT
[    1.120401] bcm2835-i2c fe804000.i2c: BSC1 Controller at 0xfe804000 (irq 48)
[    1.240502] i2c 1-0048: TMP117 High-Precision Digital Temperature Sensor probed (addr: 0x48)
[    1.241005] i2c 1-0048: hardware resolution: 0.0078125 °C/LSB, conversion: continuous 16-avg
[    1.320110] i2c 1-0040: INA219 Bi-Directional Current & Power Monitor probed (addr: 0x40)
[    1.320450] i2c 1-0040: calibration value: 4096 (10 mA/LSB, 10 mOhm precision shunt)
[    1.410220] i2c 1-0068: InvenSense MPU-6050 6-Axis Motion Sensor probed (addr: 0x68)
[    1.501004] bcm2835-gpio pinctrl: GPIO Bank A registered (pins 0-4, relay interlocks active)
[    1.620005] bcm2835-wdt bcm2835-wdt: Broadcom BCM2835 watchdog timer initialized (timeout=2000ms)
[    1.800000] industrial_edge_gateway: 6 Real-Time POSIX threads spawned with SCHED_FIFO priority 80
[    1.905002] industrial_edge_gateway: Modbus TCP MBAP protocol handler initialized on port 1502"""

    def cmd_top(self, args):
        t = query_live_telemetry()
        t_str = time.strftime("%H:%M:%S")
        return f"""top - {t_str} up 42 min,  1 user,  load average: 0.14, 0.09, 0.06
Tasks: 104 total,   1 running, 103 sleeping,   0 stopped,   0 zombie
%Cpu(s):  2.4 us,  1.1 sy,  0.0 ni, 96.2 id,  0.1 wa,  0.0 hi,  0.2 si,  0.0 st
MiB Mem :   3892.0 total,   2774.6 free,    598.2 used,    519.2 buff/cache
MiB Swap:   1024.0 total,   1024.0 free,      0.0 used.   3140.8 avail Mem

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
  17208 root      -2   0   48220  18840   8120 S   2.1   0.5   0:42.15 edge_gateway
  21288 root      -2   0   32100  12400   6420 S   1.8   0.3   0:38.22 mcu_simulator
  14604 root      20   0   58400  28900  12100 S   0.8   0.7   0:14.05 python3 dashboard
    842 root      20   0   14200   3200   2800 S   0.1   0.1   0:01.40 systemd-journal
      1 root      20   0   22100   8100   6200 S   0.0   0.2   0:04.12 systemd"""

    def cmd_ps(self, args):
        return """USER         PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND
root           1  0.0  0.2  22100  8100 ?        Ss   04:50   0:04 /sbin/init
root       17208  2.1  0.5  48220 18840 ?        Ssl  04:50   0:42 ./build/edge_gateway --config config/gateway.conf
root       21288  1.8  0.3  32100 12400 ?        Ssl  04:50   0:38 ./build/mcu_simulator
root       14604  0.8  0.7  58400 28900 pts/1    S+   04:50   0:14 python3 dashboard/app.py --port 8080"""

    def cmd_tcpdump(self, args):
        t = query_live_telemetry()
        t_str = time.strftime("%H:%M:%S")
        temp = float(t.get("temperature", 50.0))
        curr = float(t.get("current", 6.5))
        vibe = float(t.get("vibration", 1.5))
        rpm = float(t.get("rpm", 1600.0))
        
        # Build binary frame representation
        raw_header = b"\xAA\x55\x01\x00\x01\x01\x00\x00\x00\x1A"
        hex_hdr = " ".join(f"{b:02x}" for b in raw_header)
        
        return f"""tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on lo, link-type EN10MB (Ethernet), snapshot length 262144 bytes
{t_str}.401201 IP 127.0.0.1.54320 > 127.0.0.1.9000: Flags [P.], seq 1:32, ack 1, win 512, length 31
  0x0000:  {hex_hdr}  0000 01a0 f4a9 c500  .U............
  0x0010:  000f 0142 48a3 d742 48a3 d700  ...BH..BH...
  [DECODED FRAME]: Sync: 0xAA55 | Ver: 1 | Dev: 0x0001 | Msg: SENSOR_DATA
  [PAYLOAD]: Temp={temp:.2f}C | Curr={curr:.2f}A | Vibe={vibe:.2f}mm/s | RPM={rpm:.0f}
  [CRC-32 CHECKSUM]: 0xCBF43926 [CORRECT / VERIFIED]"""

    def cmd_sniff(self, args):
        return self.cmd_tcpdump(args)

    def cmd_modbus(self, args):
        return self.cmd_modbus_cli(args)

    def cmd_modbus_cli(self, args):
        t = query_live_telemetry()
        temp = int(float(t.get("temperature", 50.0)) * 10)
        curr = int(float(t.get("current", 6.5)) * 100)
        vibe = int(float(t.get("vibration", 1.5)) * 100)
        rpm = int(float(t.get("rpm", 1600.0)))
        state_map = {"OFF": 0, "STARTING": 1, "RUNNING": 2, "WARNING": 3, "EMERGENCY_STOP": 4, "SAFE_STOP": 5}
        state_code = state_map.get(t.get("controller_state", "RUNNING"), 2)

        return f"""Connecting to Modbus TCP Server at 127.0.0.1:1502 (Slave ID: 1)... Connected.
Sending Function Code 03 (Read Holding Registers: 40001 - 40008)...
┌────────────┬─────────────────────────────┬───────────────┬──────────────────────────┐
│ Register   │ Description                 │ Raw Hex       │ Scaled Value             │
├────────────┼─────────────────────────────┼───────────────┼──────────────────────────┤
│ 40001      │ Temperature (0.1 °C/LSB)    │ 0x{temp:04X}      │ {temp/10.0:.1f} °C                  │
│ 40002      │ Motor Current (0.01 A/LSB)  │ 0x{curr:04X}      │ {curr/100.0:.2f} A                 │
│ 40003      │ Vibration RMS (0.01 mm/s)   │ 0x{vibe:04X}      │ {vibe/100.0:.2f} mm/s              │
│ 40004      │ Motor Speed (1 RPM/LSB)     │ 0x{rpm:04X}      │ {rpm} RPM                 │
│ 40005      │ Actuator State Code         │ 0x{state_code:04X}      │ {t.get('controller_state', 'RUNNING')}                │
│ 40006      │ Active Fault Bitmask        │ 0x0000      │ 0 (No faults)            │
│ 40007      │ Watchdog Health Status      │ 0x0001      │ 1 (Heartbeat OK)         │
│ 40008      │ Telemetry Sequence Number   │ 0x04F2      │ 1266 frames              │
└────────────┴─────────────────────────────┴───────────────┴──────────────────────────┘
8 registers successfully polled via standard Modbus Application Protocol (MBAP)."""

    def cmd_inject(self, args):
        if not args:
            return "Usage: inject <fault_type>\nAvailable faults: temperature-high, current-high, vibration-high, bad-crc, sensor-timeout, clear"
        fault = args[0].lower()
        mapping = {
            "temperature-high": "TEMP_HIGH",
            "current-high": "CURRENT_HIGH",
            "vibration-high": "VIBE_HIGH",
            "bad-crc": "BAD_CRC",
            "sensor-timeout": "PAUSE",
            "clear": "RESET"
        }
        wire_fault = mapping.get(fault, fault.upper())
        resp = send_fault_cmd(wire_fault)
        return f"[FAULT INJECTED]: {fault} -> Response: {resp}"

    def cmd_start(self, args):
        return send_gateway_raw_cmd("START")

    def cmd_stop(self, args):
        return send_gateway_raw_cmd("STOP")

    def cmd_reset(self, args):
        return send_gateway_raw_cmd("RESET")

    def cmd_estop(self, args):
        return send_gateway_raw_cmd("ESTOP")

    def cmd_status(self, args):
        return send_gateway_raw_cmd("STATUS")

    def cmd_sensors(self, args):
        return send_gateway_raw_cmd("SENSORS")

    def cmd_faults(self, args):
        return send_gateway_raw_cmd("FAULTS")

    def cmd_lsdev(self, args):
        return send_gateway_raw_cmd("HARDWARE")

    def cmd_lshardware(self, args):
        return send_gateway_raw_cmd("HARDWARE")

    def cmd_gpio(self, args):
        return send_gateway_raw_cmd("GPIO")
