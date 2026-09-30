#!/usr/bin/env python3
"""
MQTT Telemetry Monitor for Industrial Edge Controller
Listens for messages on industrial/device01/# topics.
Supports paho-mqtt if installed, with a built-in fallback TCP subscriber.
"""

import sys
import argparse
import time

def run_with_paho(host, port, topic):
    import paho.mqtt.client as mqtt

    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print(f"[MQTT] Connected successfully to {host}:{port}")
            print(f"[MQTT] Subscribed to topic: {topic}")
            client.subscribe(topic)
        else:
            print(f"[MQTT] Connection failed with code {rc}")

    def on_message(client, userdata, msg):
        payload_str = msg.payload.decode('utf-8', errors='ignore')
        print(f"[{time.strftime('%X')}] TOPIC: {msg.topic:<35} | {payload_str}")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(host, port, 60)
        client.loop_forever()
    except Exception as e:
        print(f"[ERROR] Failed to run MQTT monitor: {e}")

def run_socket_fallback(host, port, topic_filter):
    import socket
    print(f"[MQTT] (Lightweight fallback mode without paho-mqtt)")
    print(f"[MQTT] Connecting to {host}:{port}...")

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((host, port))

        # MQTT CONNECT packet (v3.1.1)
        cid = b"mqtt_monitor_tool"
        var_header = b"\x00\x04MQTT\x04\x02\x00<\x00" + bytes([len(cid)]) + cid
        conn_pkt = b"\x10" + bytes([len(var_header)]) + var_header
        s.sendall(conn_pkt)

        # Read CONNACK
        connack = s.recv(4)
        if len(connack) < 4 or connack[0] != 0x20 or connack[3] != 0x00:
            print("[ERROR] MQTT Broker rejected connection")
            return

        print(f"[MQTT] Connected to broker! Subscribing to: {topic_filter}")

        # MQTT SUBSCRIBE packet
        topic_bytes = topic_filter.encode('utf-8')
        packet_id = 1
        sub_payload = bytes([packet_id >> 8, packet_id & 0xFF]) + \
                      bytes([len(topic_bytes) >> 8, len(topic_bytes) & 0xFF]) + \
                      topic_bytes + b"\x00" # QoS 0
        sub_pkt = b"\x82" + bytes([len(sub_payload)]) + sub_payload
        s.sendall(sub_pkt)

        # Read SUBACK
        suback = s.recv(5)
        print("[MQTT] Listening for telemetry stream (Press Ctrl+C to stop)...")

        s.settimeout(None)
        while True:
            data = s.recv(1024)
            if not data:
                break
            # Parse PUBLISH packet if present
            if (data[0] & 0xF0) == 0x30:
                # Basic parser for QoS 0 publish
                rem_len = data[1]
                t_len = (data[2] << 8) | data[3]
                topic = data[4:4 + t_len].decode('utf-8', errors='ignore')
                payload = data[4 + t_len:].decode('utf-8', errors='ignore')
                print(f"[{time.strftime('%X')}] TOPIC: {topic:<35} | {payload}")
    except Exception as e:
        print(f"[ERROR] Socket error: {e}")

def main():
    parser = argparse.ArgumentParser(description="MQTT Telemetry Monitor")
    parser.add_argument("--host", default="127.0.0.1", help="MQTT broker host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=1883, help="MQTT broker port (default: 1883)")
    parser.add_argument("--topic", default="industrial/device01/#", help="MQTT topic filter")

    args = parser.parse_args()

    try:
        import paho.mqtt.client
        run_with_paho(args.host, args.port, args.topic)
    except ImportError:
        run_socket_fallback(args.host, args.port, args.topic)

if __name__ == "__main__":
    main()
