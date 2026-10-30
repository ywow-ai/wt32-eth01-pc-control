#!/usr/bin/env bash
# Upload web UI (data/) to ESP32 LittleFS. Jalankan setelah upload sketch.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-/dev/ttyUSB0}"
ESPTOOL_BEFORE="${ESPTOOL_BEFORE:-default-reset}"
ESPTOOL_AFTER="${ESPTOOL_AFTER:-hard-reset}"
MKL=$(ls -d "$HOME/.arduino15/packages/esp32/tools/mklittlefs/"*/mklittlefs 2>/dev/null | sort -V | tail -1)
ESPTOOL=$(ls -d "$HOME/.arduino15/packages/esp32/tools/esptool_py/"*/esptool 2>/dev/null | sort -V | tail -1)

if [[ ! -x "$MKL" || ! -x "$ESPTOOL" ]]; then
  echo "mklittlefs atau esptool tidak ditemukan. Install core esp32 via arduino-cli."
  exit 1
fi

BIN="/tmp/wt32-eth01-littlefs.bin"
LFS_SIZE=0x160000
LFS_OFFSET=0x290000
"$MKL" -c "$ROOT/data" -s "$LFS_SIZE" "$BIN"
"$ESPTOOL" --chip esp32 --port "$PORT" --before "$ESPTOOL_BEFORE" --after "$ESPTOOL_AFTER" write-flash -fs 4MB "$LFS_OFFSET" "$BIN"
echo "LittleFS upload selesai."
