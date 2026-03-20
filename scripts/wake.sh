#!/usr/bin/env bash
# wake.sh — Wake the Windows PC remotely via Tailscale → Pi → Arduino
# Usage: ./scripts/wake.sh [command]
#   Commands:
#     wake          (default) power pulse + wait for boot + type PIN
#     power         power button pulse only
#     type-password type PIN only (PC already on)
#     status        check Pi and Arduino connectivity

set -euo pipefail

# ── Configuration ────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${SCRIPT_DIR}/../.env"

# Load .env if present
if [ -f "${ENV_FILE}" ]; then
  set -a
  source "${ENV_FILE}"
  set +a
fi

PI_TAILSCALE_IP="${PI_TAILSCALE_IP:?Set PI_TAILSCALE_IP in .env}"
PI_PORT="${PI_PORT:-5000}"
BASE_URL="http://${PI_TAILSCALE_IP}:${PI_PORT}"

CMD="${1:-wake}"

# ── Helpers ──────────────────────────────────────────────────────
info()    { echo "[info]  $*"; }
success() { echo "[ok]    $*"; }
err()     { echo "[error] $*" >&2; exit 1; }

check_deps() {
  for dep in curl jq; do
    command -v "${dep}" &>/dev/null || err "${dep} not found. Install with: brew install ${dep}"
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
    "${BASE_URL}${endpoint}" 2>/dev/null) || err "Cannot reach Pi (${PI_TAILSCALE_IP}). Is Tailscale up?"

  http_code=$(echo "${resp}" | tail -n1)
  body=$(echo "${resp}" | head -n-1)

  if [[ "${http_code}" =~ ^2 ]]; then
    echo "${body}" | jq . 2>/dev/null || echo "${body}"
  else
    err "Pi responded HTTP ${http_code}: ${body}"
  fi
}

# ── Commands ─────────────────────────────────────────────────────
cmd_status() {
  info "Checking Pi and Arduino status..."
  call_pi "/status" "GET" 10
  success "All reachable."
}

cmd_power() {
  info "Sending power button pulse..."
  call_pi "/power" "POST" 10
  success "Pulse sent."
}

cmd_type_password() {
  info "Typing PIN..."
  call_pi "/type-password" "POST" 10
  success "PIN sent."
}

cmd_wake() {
  info "Starting full wake sequence..."
  info "  1. Power button pulse"
  info "  2. Waiting for Windows to boot (~35s)"
  info "  3. Typing PIN"
  call_pi "/wake" "POST" 15
  success "Wake sequence started. PC should be ready in ~35 seconds."
  echo ""
  echo "Connect via RDP once the PC is up:"
  echo "  open rdp://<windows-pc-ip>"
}

# ── Main ─────────────────────────────────────────────────────────
check_deps

case "${CMD}" in
  wake)           cmd_wake ;;
  power)          cmd_power ;;
  type-password)  cmd_type_password ;;
  status)         cmd_status ;;
  *)
    echo "Usage: $0 [wake|power|type-password|status]"
    echo "  wake          (default) full sequence"
    echo "  power         power button pulse only"
    echo "  type-password type PIN only"
    echo "  status        check connectivity"
    exit 1
    ;;
esac
