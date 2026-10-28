#!/usr/bin/env bash
# Upload web UI (data/) to ESP32 LittleFS. Jalankan setelah upload sketch.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-/dev/ttyUSB0}"
FQBN="esp32:esp32:wt32-eth01"
MKL=$(ls -d "$HOME/.arduino15/packages/esp32/tools/mklittlefs/"*/mklittlefs 2>/dev/null | sort -V | tail -1)
ESPTOOL=$(ls -d "$HOME/.arduino15/packages/esp32/tools/esptool_py/"*/esptool 2>/dev/null | sort -V | tail -1)

if [[ ! -x "$MKL" || ! -x "$ESPTOOL" ]]; then
  echo "mklittlefs atau esptool tidak ditemukan. Install core esp32 via arduino-cli."
  exit 1
fi

BIN="/tmp/wt32-eth01-littlefs.bin"
"$MKL" -c "$ROOT/data" -s 0x190000 "$BIN"
# Offset littlefs default partition (0x290000) — sesuaikan jika ganti partition table
"$ESPTOOL" --chip esp32 --port "$PORT" --before default-reset --after hard-reset write-flash 0x290000 "$BIN"
echo "LittleFS upload selesai."
