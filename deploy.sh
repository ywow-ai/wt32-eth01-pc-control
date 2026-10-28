#!/usr/bin/env bash
# Production deploy: compile, flash sketch + LittleFS, apply ./.env to NVS.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FQBN="${FQBN:-esp32:esp32:wt32-eth01}"
WT32_PORT="${WT32_PORT:-/dev/ttyUSB0}"
C3_PORT="${C3_PORT:-/dev/ttyACM0}"

echo "[deploy] compile ..."
arduino-cli compile --fqbn "$FQBN" "$ROOT"

echo "[deploy] flash sketch (C3 boot if ports present) ..."
if [[ -e "$C3_PORT" && -e "$WT32_PORT" ]]; then
  "$ROOT/flash_wt32.sh" upload "$WT32_PORT" "$C3_PORT"
else
  arduino-cli upload -p "$WT32_PORT" --fqbn "$FQBN" "$ROOT"
fi

echo "[deploy] LittleFS ..."
if [[ -e "$C3_PORT" && -e "$WT32_PORT" ]]; then
  "$ROOT/flash_wt32.sh" upload-fs "$WT32_PORT" "$C3_PORT"
else
  "$ROOT/upload_littlefs.sh" "$WT32_PORT"
fi

echo "[deploy] NVS from .env ..."
"$ROOT/apply_env.sh"

echo "[deploy] done — buka URL dari [alive]; ganti password web jika masih default."
