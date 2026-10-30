#!/usr/bin/env bash
# Smoke + API tests for wt32-eth01-pc-control (production checklist).
# Usage: ./test_production.sh [base_url]   default: http://192.168.1.201
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="${1:-http://192.168.1.201}"
BASE="${BASE%/}"
PORT="${WT32_SERIAL_PORT:-/dev/ttyUSB0}"
PASS="${WEB_PASS:-12345678}"
COOKIE=$(mktemp)
trap 'rm -f "$COOKIE"' EXIT

pass=0
fail=0
ok() { echo "  OK  $1"; pass=$((pass + 1)); }
bad() { echo "  FAIL $1"; fail=$((fail + 1)); }

echo "[test] base=$BASE serial=$PORT"

echo "[test] compile ..."
if arduino-cli compile --fqbn esp32:esp32:wt32-eth01 "$ROOT" >/dev/null 2>&1; then
  ok "arduino-cli compile"
else
  bad "arduino-cli compile"
fi

echo "[test] ping ..."
if ping -c 1 -W 2 "${BASE#http://}" >/dev/null 2>&1; then
  ok "ping ${BASE#http://}"
else
  bad "ping ${BASE#http://} (LAN/Wi-Fi belum up?)"
fi

echo "[test] HTTP static + login flow ..."
code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE/login.html" || echo 0)
[[ "$code" == "200" ]] && ok "GET /login.html" || bad "GET /login.html ($code)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -c "$COOKIE" -X POST \
  -d "password=wrong" "$BASE/api/login" || echo 0)
[[ "$code" == "401" ]] && ok "POST /api/login bad pass" || bad "POST /api/login bad pass ($code)"

code=$(curl -sS -o /dev/null -w '%{http_code}' -b "$COOKIE" -c "$COOKIE" -X POST \
  -d "password=$PASS" "$BASE/api/login" || echo 0)
[[ "$code" == "200" ]] && ok "POST /api/login" || bad "POST /api/login ($code)"

body=$(curl -sS -b "$COOKIE" "$BASE/api/session" || true)
echo "$body" | grep -q '"ok":true' && ok "GET /api/session" || bad "GET /api/session"

body=$(curl -sS -b "$COOKIE" "$BASE/api/status" || true)
echo "$body" | grep -q '"ok":true' && ok "GET /api/status" || bad "GET /api/status"
echo "$body" | grep -q 'active_if' && ok "status has active_if" || bad "status active_if"

body=$(curl -sS -b "$COOKIE" "$BASE/api/config" || true)
echo "$body" | grep -q '"ok":true' && ok "GET /api/config" || bad "GET /api/config"

for path in /css/app.css /js/app.js /index.html; do
  code=$(curl -sS -o /dev/null -w '%{http_code}' -b "$COOKIE" "$BASE$path" || echo 0)
  [[ "$code" == "200" ]] && ok "GET $path" || bad "GET $path ($code)"
done

code=$(curl -sS -o /dev/null -w '%{http_code}' "$BASE/api/status" || echo 0)
[[ "$code" == "401" ]] && ok "GET /api/status no cookie" || bad "GET /api/status unauth ($code)"

echo "[test] serial CLI ..."
if [[ -e "$PORT" ]]; then
  python3 - <<PY
import serial, time
port = "$PORT"
s = serial.Serial(port, 115200, timeout=0.3)
def cmd(line, wait=0.35):
    s.write((line + "\n").encode())
    time.sleep(wait)
    out = b""
    t = time.time()
    while time.time() - t < 0.8:
        out += s.read(512)
    return out.decode("utf-8", errors="replace")

cmd("help")
h = cmd("help")
assert "[SERIAL]" in h or "help" in h.lower(), "help missing"
print("  OK  serial help")

cmd("show")
sh = cmd("show", 0.5)
assert "net_link" in sh or "[SERIAL]" in sh, "show missing net_link"
print("  OK  serial show")

g = cmd("get net_link", 0.55)
if "net_link" not in g and "[SERIAL] ERR" in g:
    raise AssertionError("get net_link failed")
print("  OK  serial get net_link")
s.close()
PY
  pass=$((pass + 3))
else
  bad "serial port $PORT missing"
fi

echo
echo "[test] summary: ok=$pass fail=$fail"
if [[ "$fail" -gt 0 ]]; then
  echo "[test] PRODUCTION CHECK — ada gagal (perbaiki sebelum deploy lapangan)"
  exit 1
fi
echo "[test] PRODUCTION CHECK — lulus smoke (tes opto/Pusher/Discord manual jika dipakai)"
exit 0
