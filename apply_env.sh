#!/usr/bin/env bash
# Apply ./.env to WT32 NVS via serial (115200), then reboot and print web URL from [alive].
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="${ENV_FILE:-$ROOT/.env}"

if [[ ! -f "$ENV_FILE" ]]; then
  echo "Missing $ENV_FILE — copy from .env.example"
  exit 1
fi

set -a
# shellcheck source=/dev/null
source "$ENV_FILE"
set +a

PORT="${WT32_SERIAL_PORT:-/dev/ttyUSB0}"
C3_PORT="${C3_PORT:-/dev/ttyACM0}"
C3_EN="${C3_EN:-io3}"
EN_MS="${EN_MS:-150}"
BAUD=115200

if [[ ! -e "$PORT" ]]; then
  echo "Serial port not found: $PORT"
  exit 1
fi

c3_pulse_en() {
  [[ -e "$C3_PORT" ]] || return 0
  stty -F "$C3_PORT" "$BAUD" cs8 -cstopb -parenb raw -echo min 0 time 0 2>/dev/null || true
  printf 'high %s %s\n' "$C3_EN" "$EN_MS" >"$C3_PORT"
  sleep 0.2
}

stty -F "$PORT" "$BAUD" cs8 -cstopb -parenb raw -echo 2>/dev/null || true

serial_cmd() {
  printf '%s\n' "$1" >"$PORT"
  sleep 0.18
}

serial_set() {
  serial_cmd "set $1 $2"
}

echo "[env] applying to WT32 on $PORT ..."

NET="${NET_LINK:-auto}"
if [[ -n "${WLAN_SSID:-}" ]]; then
  [[ "$NET" == "auto" ]] && NET="wifi"
fi
serial_set net_link "$NET"

if [[ -n "${WLAN_SSID:-}" ]]; then
  serial_set wifi_ssid "$WLAN_SSID"
  [[ -n "${WLAN_PASS:-}" ]] && serial_set wifi_pass "$WLAN_PASS"
fi

IPM="${IP_MODE:-dhcp}"
if [[ -n "${IP_ADDR:-}" && "$IPM" == "dhcp" ]]; then
  IPM="static"
fi
serial_set ip_mode "$IPM"
[[ -n "${IP_ADDR:-}" ]] && serial_set ip_addr "$IP_ADDR"
[[ -n "${SUBNET:-}" ]] && serial_set subnet "$SUBNET"
[[ -n "${GATEWAY:-}" ]] && serial_set gateway "$GATEWAY"
[[ -n "${DNS1:-}" ]] && serial_set dns1 "$DNS1"
[[ -n "${DNS2:-}" ]] && serial_set dns2 "$DNS2"

if [[ -n "${PUSHER_CHANNEL:-}" ]]; then
  serial_set pusher_chan "$PUSHER_CHANNEL"
fi
if [[ -n "${PUSHER_WS_HOST:-}" ]]; then
  serial_set ws_host "$PUSHER_WS_HOST"
fi
if [[ -n "${PUSHER_WS_PORT:-}" ]]; then
  serial_set ws_port "$PUSHER_WS_PORT"
fi
if [[ -n "${KEY:-}" ]]; then
  serial_set pusher_key "$KEY"
fi
if [[ -n "${CLUSTER:-}" ]]; then
  serial_set pusher_cluster "$CLUSTER"
fi

if [[ -n "${DISCORD_WEBHOOK_ID:-}" && -n "${DISCORD_TOKEN:-}" ]]; then
  DISCORD_URL="https://discord.com/api/webhooks/${DISCORD_WEBHOOK_ID}/${DISCORD_TOKEN}"
  serial_set discord_en "1"
  serial_set discord_url "$DISCORD_URL"
  [[ -n "${DISCORD_USER:-}" ]] && serial_set discord_user "$DISCORD_USER"
  [[ -n "${DISCORD_EVENTS:-}" ]] && serial_set discord_ev "$DISCORD_EVENTS"
fi

CPU_MHZ="${CPU_MHZ:-160}"
LOOP_DELAY_MS="${LOOP_DELAY_MS:-5}"
serial_set cpu_mhz "$CPU_MHZ"
serial_cmd "set loop_delay_ms $LOOP_DELAY_MS"

echo "[env] reboot WT32 ..."
serial_cmd reboot
sleep 0.15
c3_pulse_en

echo "[env] waiting for [alive] (up to 45s) ..."
python3 - "$PORT" "$BAUD" <<'PY'
import re, sys, time, serial

port, baud = sys.argv[1], int(sys.argv[2])
pat = re.compile(r"\[alive\]\s+(https?://[^\s]+)")
deadline = time.time() + 45
buf = ""
found = []
try:
    s = serial.Serial(port, baud, timeout=0.3)
except Exception as e:
    print(f"[env] serial open failed: {e}", file=sys.stderr)
    sys.exit(1)

while time.time() < deadline:
    chunk = s.read(4096).decode("utf-8", errors="replace")
    if chunk:
        buf += chunk
        for m in pat.finditer(buf):
            u = m.group(1).rstrip("/")
            if u not in found:
                found.append(u)
                print(f"[env] URL: {u}/")
    if found and time.time() > deadline - 35:
        break
    time.sleep(0.1)

s.close()
if not found:
    print("[env] no [alive] yet — cek serial: ETH/WIFI IP, net_link, kabel/Wi-Fi", file=sys.stderr)
    sys.exit(2)
PY

echo "[env] done. Login web: password default 12345678 (ganti setelah login)."
