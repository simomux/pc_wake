#!/usr/bin/env python3
"""
Gateway Flask sul Raspberry Pi Zero 2W.
Riceve comandi via Tailscale e li forwarda all'Arduino via HTTP locale.
"""

import os
import requests
from flask import Flask, jsonify, abort

app = Flask(__name__)

ARDUINO_IP  = os.environ.get("ARDUINO_IP") or exit("ARDUINO_IP non impostato nel .env")
ARDUINO_URL = f"http://{ARDUINO_IP}"
TIMEOUT     = 5  # secondi per chiamate rapide (power, status)
WAKE_TIMEOUT = 10  # secondi — la risposta /wake arriva subito, ma la rete può essere lenta


def call_arduino(endpoint: str, method: str = "POST", timeout: int = TIMEOUT):
    url = f"{ARDUINO_URL}{endpoint}"
    try:
        resp = requests.request(method, url, timeout=timeout)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ConnectionError:
        abort(502, description=f"Arduino non raggiungibile ({ARDUINO_IP})")
    except requests.exceptions.Timeout:
        abort(504, description="Timeout chiamata Arduino")
    except requests.exceptions.HTTPError as e:
        abort(502, description=str(e))


# ── Endpoints ────────────────────────────────────────────────────

@app.get("/status")
def status():
    """Verifica che Pi e Arduino siano raggiungibili."""
    arduino_status = call_arduino("/status", method="GET")
    return jsonify({"pi": "ok", "arduino": arduino_status})


@app.post("/power")
def power():
    """Pulse power button (pressione singola da 200ms)."""
    result = call_arduino("/power")
    return jsonify(result)


@app.post("/type-password")
def type_password():
    """Digita la password sul PC (da usare se già acceso)."""
    result = call_arduino("/type-password")
    return jsonify(result)


@app.post("/wake")
def wake():
    """
    Wake completo: power pulse → attesa boot → digitazione password.
    L'Arduino risponde subito (202) e poi esegue in background.
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
    # Ascolta su tutte le interfacce (Tailscale inclusa), porta 5000
    app.run(host="0.0.0.0", port=5000, debug=False)
