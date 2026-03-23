#!/usr/bin/env python3
"""
Flask gateway on the Raspberry Pi Zero 2W.
Receives commands over Tailscale and forwards them to the Arduino via LAN HTTP.
"""

import os
import subprocess
import time
import requests
from flask import Flask, jsonify, abort, request

app = Flask(__name__)

ARDUINO_IP  = os.environ.get("ARDUINO_IP")  or exit("ARDUINO_IP not set in .env")
WIN_PC_IP   = os.environ.get("WIN_PC_IP")   or exit("WIN_PC_IP not set in .env")
API_SECRET  = os.environ.get("API_SECRET")  or exit("API_SECRET not set in .env")
ARDUINO_URL = f"http://{ARDUINO_IP}"


@app.before_request
def check_auth():
    token = request.headers.get("Authorization", "")
    if token != f"Bearer {API_SECRET}":
        abort(401, description="Unauthorized")

TIMEOUT       = 5    # seconds, for quick calls
PING_INTERVAL = 3    # seconds between ping attempts
PING_TIMEOUT  = 90   # seconds max wait for PC to come up


def call_arduino(endpoint: str, method: str = "POST", timeout: int = TIMEOUT):
    url = f"{ARDUINO_URL}{endpoint}"
    try:
        resp = requests.request(method, url, timeout=timeout)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ConnectionError:
        abort(502, description=f"Arduino unreachable ({ARDUINO_IP})")
    except requests.exceptions.Timeout:
        abort(504, description="Arduino request timed out")
    except requests.exceptions.HTTPError as e:
        abort(502, description=str(e))


def wait_for_pc(timeout: int = PING_TIMEOUT) -> bool:
    """Ping WIN_PC_IP every PING_INTERVAL seconds until it responds or timeout."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        result = subprocess.run(
            ["ping", "-c", "1", "-W", "1", WIN_PC_IP],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        if result.returncode == 0:
            return True
        time.sleep(PING_INTERVAL)
    return False


# ── Endpoints ────────────────────────────────────────────────────

@app.get("/status")
def status():
    """Check that both Pi and Arduino are reachable."""
    arduino_status = call_arduino("/status", method="GET")
    return jsonify({"pi": "ok", "arduino": arduino_status})


@app.post("/power")
def power():
    """Single 200ms power button pulse."""
    result = call_arduino("/power")
    return jsonify(result)


@app.post("/wake")
def wake():
    """
    Full wake sequence: power pulse → wait for PC to respond to ping → return ready.
    Blocks until the PC is reachable (up to PING_TIMEOUT seconds).
    """
    call_arduino("/power")

    if not wait_for_pc():
        abort(504, description=f"PC did not come up within {PING_TIMEOUT}s")

    return jsonify({"status": "ready", "pc": WIN_PC_IP})


# ── Error handlers ───────────────────────────────────────────────

@app.errorhandler(401)
@app.errorhandler(502)
@app.errorhandler(504)
def gateway_error(e):
    return jsonify({"error": str(e.description)}), e.code


# ── Main ─────────────────────────────────────────────────────────

if __name__ == "__main__":
    # Listen on all interfaces (Tailscale included), port 5000
    app.run(host="0.0.0.0", port=5000, debug=False)
