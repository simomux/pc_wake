#!/usr/bin/env bash
# Automated setup for Raspberry Pi Zero 2W
# Run as root: sudo bash install.sh
set -euo pipefail

# ── Variables (from .env if present) ────────────────────────────
if [ -f /home/pi/pc-remote/.env ]; then
  set -a
  source /home/pi/pc-remote/.env
  set +a
fi

ARDUINO_IP="${ARDUINO_IP:?Set ARDUINO_IP in .env before running this script}"
APP_DIR="/home/pi/pc-remote/pi"
SERVICE_NAME="pc-remote"

echo "=== [1/5] Updating system ==="
apt-get update -qq
apt-get install -y -qq python3 python3-pip python3-venv curl

echo "=== [2/5] Installing Tailscale ==="
if ! command -v tailscale &>/dev/null; then
  curl -fsSL https://tailscale.com/install.sh | sh
else
  echo "Tailscale already installed, skipping."
fi

echo "=== [3/5] Enabling IP forwarding (subnet router) ==="
grep -qxF 'net.ipv4.ip_forward=1' /etc/sysctl.conf \
  || echo 'net.ipv4.ip_forward=1' >> /etc/sysctl.conf
grep -qxF 'net.ipv6.conf.all.forwarding=1' /etc/sysctl.conf \
  || echo 'net.ipv6.conf.all.forwarding=1' >> /etc/sysctl.conf
sysctl -p -q

echo "=== [4/5] Setting up Flask virtualenv ==="
python3 -m venv "${APP_DIR}/venv"
"${APP_DIR}/venv/bin/pip" install -q --upgrade pip
"${APP_DIR}/venv/bin/pip" install -q -r "${APP_DIR}/requirements.txt"

echo "=== [5/5] Installing systemd service ==="
cat > /etc/systemd/system/${SERVICE_NAME}.service <<EOF
[Unit]
Description=pc-remote Flask gateway
After=network-online.target tailscaled.service
Wants=network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=${APP_DIR}
EnvironmentFile=/home/pi/pc-remote/.env
ExecStart=${APP_DIR}/venv/bin/python ${APP_DIR}/app.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable --now ${SERVICE_NAME}.service

echo ""
echo "=== Setup complete ==="
echo ""
echo "Next manual steps:"
echo "  1. Start Tailscale as subnet router:"
echo "     sudo tailscale up --advertise-routes=192.168.1.0/24 --accept-dns=false"
echo "  2. Approve the subnet route at https://login.tailscale.com/admin/machines"
echo "  3. Check service:  systemctl status ${SERVICE_NAME}"
echo "  4. Local test:     curl http://localhost:5000/status"
