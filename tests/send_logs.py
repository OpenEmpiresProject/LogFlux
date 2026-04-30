#!/usr/bin/env python3
"""
Simple test script to send newline-delimited JSON log lines to the TCP server.

Usage:
    python3 tests/send_logs.py [--host HOST] [--port PORT] [--file FILE]

Defaults:
    host = localhost
    port = 5000

If --file is provided the script will read that file and send each non-empty
line at a steady rate (1 line / second). If no file is provided a small set
of example JSON lines will be sent at the same rate.
"""
import socket
import sys
import time
import json
import argparse
from pathlib import Path

SEND_INTERVAL_SECONDS = 1.0

def send_lines(sock: socket.socket, lines):
    try:
        for line in lines:
            if isinstance(line, dict):
                raw = json.dumps(line, separators=(",", ":"))
            else:
                raw = str(line).rstrip("\n")
            if not raw:
                continue
            payload = (raw + "\n").encode("utf-8")
            sock.sendall(payload)
            print("Sent:", raw)
            time.sleep(SEND_INTERVAL_SECONDS)
    except KeyboardInterrupt:
        print("\nInterrupted by user, closing socket.")
    except Exception as ex:
        print("Error while sending:", ex)

def read_file_lines(path: Path):
    if not path.exists():
        raise FileNotFoundError(f"File not found: {path}")
    with path.open("r", encoding="utf-8") as fh:
        # Keep original lines as-is (trim trailing newline), but skip empty lines
        return [line.rstrip("\n") for line in (l for l in fh) if line.strip()]

def main():
    parser = argparse.ArgumentParser(description="Send newline-delimited JSON lines to a TCP server.")
    parser.add_argument("--host", "-H", default="localhost", help="Server host (default: localhost)")
    parser.add_argument("--port", "-p", type=int, default=5000, help="Server port (default: 5000)")
    parser.add_argument("--file", "-f", help="Path to local file with lines to send (one per line)")
    args = parser.parse_args()

    host = args.host
    port = args.port

    if args.file:
        try:
            lines = read_file_lines(Path(args.file))
            if not lines:
                print(f"No non-empty lines found in {args.file}. Exiting.")
                return
        except Exception as ex:
            print("Failed to read file:", ex)
            return
    else:
        # default example JSON objects
        lines = [
            {"timestamp":"2026-04-29T12:00:00Z","level":"INFO","message":"Startup complete"},
            {"level":"WARN","message":"Slow response","path":"/api/items"},
            {"level":"ERROR","message":"Unhandled exception","exception":"ValueError","code":42},
        ]

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    try:
        print(f"Connecting to {host}:{port}...")
        s.connect((host, port))
        print("Connected. Sending lines at 1 line/sec.")
        send_lines(s, lines)
    except Exception as ex:
        print("Connection error:", ex)
    finally:
        try:
            s.close()
        except Exception:
            pass
        print("Done. Socket closed.")

if __name__ == "__main__":
    main()