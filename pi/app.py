#!/usr/bin/env python3
"""
Flask gateway on the Raspberry Pi Zero 2W.
Receives commands over Tailscale and forwards them to the Arduino via LAN HTTP.
"""

import os
import requests
from flask import Flask, jsonify, abort

app = Flask(__name__)

ARDUINO_IP   = os.environ.get("ARDUINO_IP") or exit("ARDUINO_IP not set in .env")
ARDUINO_URL  = f"http://{ARDUINO_IP}"
TIMEOUT      = 5   # seconds, for quick calls (power, status)
WAKE_TIMEOUT = 10  # seconds — /wake responds immediately but network may be slow


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


@app.post("/type-password")
def type_password():
    """Type the Windows PIN via HID (use when PC is already on)."""
    result = call_arduino("/type-password")
    return jsonify(result)


@app.post("/wake")
def wake():
    """
    Full wake sequence: power pulse → wait for boot → type PIN.
    Arduino responds immediately; the sequence runs on the Arduino side.
    """
    result = call_arduino("/wake", timeout=WAKE_TIMEOUT)
    return jsonify(result), 202


# ── Error handlers ───────────────────────────────────────────────

@app.errorhandler(502)
@app.errorhandler(504)
def gateway_error(e):
    return jsonify({"error": str(e.description)}), e.code


# ── Main ─────────────────────────────────────────────────────────

if __name__ == "__main__":
    # Listen on all interfaces (Tailscale included), port 5000
    app.run(host="0.0.0.0", port=5000, debug=False)
