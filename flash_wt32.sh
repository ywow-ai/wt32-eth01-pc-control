#!/usr/bin/env bash
# WT32 upload: FT232H (data) + ESP32-C3 esp32c3-gpio (GPIO generic serial).
# Wiring: C3 io1 → WT32 GPIO0, C3 io3 → WT32 EN, GND common.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-upload}"
WT32_PORT="${WT32_PORT:-${2:-/dev/ttyUSB0}}"
C3_PORT="${C3_PORT:-${3:-/dev/ttyACM0}}"
C3_FLASH="${C3_FLASH:-io1}"
C3_EN="${C3_EN:-io3}"
EN_MS="${EN_MS:-200}"
FQBN="${FQBN:-esp32:esp32:wt32-eth01}"

# esptool must not toggle FT232H DTR/RTS — boot is via C3 only.
ESPTOOL_ARGS='--chip esp32 --port {serial.port} --baud {upload.speed} {upload.flags} --before no-reset --after hard-reset write-flash {upload.erase_cmd} -z --flash-mode keep --flash-freq keep --flash-size keep {build.bootloader_addr} "{build.path}/{build.project_name}.bootloader.bin" 0x8000 "{build.path}/{build.project_name}.partitions.bin" 0xe000 "{runtime.platform.path}/tools/partitions/boot_app0.bin" 0x10000 "{build.path}/{build.project_name}.bin" {upload.extra_flags}'

[[ "$ACTION" == "upload" || "$ACTION" == "upload-fs" ]] || {
  echo "Usage: $0 [upload|upload-fs] [wt32_port] [c3_port]"
  exit 1
}

for p in "$C3_PORT" "$WT32_PORT"; do
  [[ -e "$p" ]] || {
    echo "Missing port: $p"
    exit 1
  }
done

c3() {
  stty -F "$C3_PORT" 115200 cs8 -cstopb -parenb raw -echo 2>/dev/null || true
  printf '%s\n' "$1" >"$C3_PORT"
  sleep 0.15
}

wt32_download() {
  c3 "low ${C3_FLASH}"
  sleep 0.08
  c3 "high ${C3_FLASH}"
  sleep 0.1
  c3 "high ${C3_EN} ${EN_MS}"
  sleep 0.12
  c3 "high ${C3_EN} ${EN_MS}"
  sleep 0.55
}

wt32_run() {
  c3 "low ${C3_FLASH}"
  sleep 0.08
  c3 "high ${C3_EN} ${EN_MS}"
  sleep 0.25
}

echo "[wt32] C3 ${C3_FLASH}=GPIO0 ${C3_EN}=EN (no-reset esptool)"
wt32_download

UPLOAD_PROP=(--upload-property "tools.esptool_py.upload.pattern_args=${ESPTOOL_ARGS}")

if [[ "$ACTION" == "upload-fs" ]]; then
  "$ROOT/upload_littlefs.sh" "$WT32_PORT"
else
  arduino-cli upload -p "$WT32_PORT" --fqbn "$FQBN" "$ROOT" "${UPLOAD_PROP[@]}"
fi

wt32_run
echo "[wt32] done"
