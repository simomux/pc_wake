#!/usr/bin/env bash
# Setup automatico Raspberry Pi Zero 2W
# Eseguire come root: sudo bash install.sh
set -euo pipefail

# ── Variabili (da .env se presente) ─────────────────────────────
if [ -f /home/pi/pc-remote/.env ]; then
  set -a
  source /home/pi/pc-remote/.env
  set +a
fi

ARDUINO_IP="${ARDUINO_IP:-192.168.1.20}"
APP_DIR="/home/pi/pc-remote/pi"
SERVICE_NAME="pc-remote"

echo "=== [1/5] Aggiornamento sistema ==="
apt-get update -qq
apt-get install -y -qq python3 python3-pip python3-venv curl

echo "=== [2/5] Installazione Tailscale ==="
if ! command -v tailscale &>/dev/null; then
  curl -fsSL https://tailscale.com/install.sh | sh
else
  echo "Tailscale già installato, skip."
fi

echo "=== [3/5] Abilitazione IP forwarding (subnet router) ==="
grep -qxF 'net.ipv4.ip_forward=1' /etc/sysctl.conf \
  || echo 'net.ipv4.ip_forward=1' >> /etc/sysctl.conf
grep -qxF 'net.ipv6.conf.all.forwarding=1' /etc/sysctl.conf \
  || echo 'net.ipv6.conf.all.forwarding=1' >> /etc/sysctl.conf
sysctl -p -q

echo "=== [4/5] Setup virtualenv Flask ==="
python3 -m venv "${APP_DIR}/venv"
"${APP_DIR}/venv/bin/pip" install -q --upgrade pip
"${APP_DIR}/venv/bin/pip" install -q -r "${APP_DIR}/requirements.txt"

echo "=== [5/5] Installazione servizio systemd ==="
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
echo "=== Setup completato ==="
echo ""
echo "Prossimi step manuali:"
echo "  1. Avvia Tailscale come subnet router:"
echo "     sudo tailscale up --advertise-routes=192.168.1.0/24 --accept-dns=false"
echo "  2. Approva il subnet router su https://login.tailscale.com/admin/machines"
echo "  3. Verifica servizio: systemctl status ${SERVICE_NAME}"
echo "  4. Test locale:       curl http://localhost:5000/status"
