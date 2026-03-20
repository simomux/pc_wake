#!/usr/bin/env bash
# wake.sh — Sveglia il PC Windows da remoto via Tailscale → Pi → Arduino
# Uso: ./scripts/wake.sh [comando]
#   Comandi disponibili:
#     wake          (default) power + attesa boot + password
#     power         solo pulse power button
#     type-password solo digitazione password (PC già acceso)
#     status        verifica che Pi e Arduino siano raggiungibili

set -euo pipefail

# ── Configurazione ───────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${SCRIPT_DIR}/../.env"

# Carica .env se esiste
if [ -f "${ENV_FILE}" ]; then
  set -a
  source "${ENV_FILE}"
  set +a
fi

PI_TAILSCALE_IP="${PI_TAILSCALE_IP:?Imposta PI_TAILSCALE_IP nel file .env}"
PI_PORT="${PI_PORT:-5000}"
BASE_URL="http://${PI_TAILSCALE_IP}:${PI_PORT}"

CMD="${1:-wake}"

# ── Helpers ──────────────────────────────────────────────────────
info()    { echo "[info]  $*"; }
success() { echo "[ok]    $*"; }
err()     { echo "[error] $*" >&2; exit 1; }

check_deps() {
  for dep in curl jq; do
    command -v "${dep}" &>/dev/null || err "${dep} non trovato. Installa con: brew install ${dep}"
  done
}

call_pi() {
  local endpoint="$1"
  local method="${2:-POST}"
  local timeout="${3:-15}"

  info "→ ${method} ${BASE_URL}${endpoint}"
  local resp http_code

  resp=$(curl -s -w "\n%{http_code}" \
    -X "${method}" \
    --connect-timeout 10 \
    --max-time "${timeout}" \
    "${BASE_URL}${endpoint}" 2>/dev/null) || err "Impossibile raggiungere il Pi (${PI_TAILSCALE_IP}). Tailscale attivo?"

  http_code=$(echo "${resp}" | tail -n1)
  body=$(echo "${resp}" | head -n-1)

  if [[ "${http_code}" =~ ^2 ]]; then
    echo "${body}" | jq . 2>/dev/null || echo "${body}"
  else
    err "Il Pi ha risposto HTTP ${http_code}: ${body}"
  fi
}

# ── Comandi ──────────────────────────────────────────────────────
cmd_status() {
  info "Verifica stato Pi e Arduino..."
  call_pi "/status" "GET" 10
  success "Tutto raggiungibile."
}

cmd_power() {
  info "Invio pulse power button..."
  call_pi "/power" "POST" 10
  success "Pulse inviato."
}

cmd_type_password() {
  info "Digitazione password in corso..."
  call_pi "/type-password" "POST" 10
  success "Password inviata."
}

cmd_wake() {
  info "Avvio sequenza wake completa..."
  info "  1. Pulse power button"
  info "  2. Attesa boot Windows (~35s)"
  info "  3. Digitazione password"
  call_pi "/wake" "POST" 15
  success "Sequenza wake avviata. Il PC sarà pronto tra ~35 secondi."
  echo ""
  echo "Per connetterti via RDP:"
  echo "  open rdp://192.168.1.10"
  echo "  (oppure usa Microsoft Remote Desktop e inserisci 192.168.1.10)"
}

# ── Main ─────────────────────────────────────────────────────────
check_deps

case "${CMD}" in
  wake)           cmd_wake ;;
  power)          cmd_power ;;
  type-password)  cmd_type_password ;;
  status)         cmd_status ;;
  *)
    echo "Uso: $0 [wake|power|type-password|status]"
    echo "  wake          (default) sequenza completa"
    echo "  power         solo pulse power button"
    echo "  type-password solo digitazione password"
    echo "  status        verifica connettività"
    exit 1
    ;;
esac
