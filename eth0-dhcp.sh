#!/usr/bin/env bash
# DHCP server on laptop eth0 for direct link to WT32-ETH01 (ESP DHCP client).
# Usage: sudo ./eth0-dhcp.sh
set -euo pipefail

IF=eth0
GATEWAY=192.168.99.1
RANGE_START=192.168.99.50
RANGE_END=192.168.99.150

if [[ "${EUID:-0}" -ne 0 ]]; then
  echo "Run: sudo $0"
  exit 1
fi

if ! ip link show "$IF" &>/dev/null; then
  echo "Interface $IF not found"
  exit 1
fi

pkill -f "dnsmasq.*interface=${IF}" 2>/dev/null || true
sleep 0.5

ip addr flush dev "$IF"
ip addr add "${GATEWAY}/24" dev "$IF"
ip link set "$IF" up

echo "eth0: ${GATEWAY}/24"
echo "DHCP range: ${RANGE_START} - ${RANGE_END}"
echo "Colok LAN ke ESP, reset board, watch serial for [ETH] IP"
echo "Ctrl+C to stop dnsmasq"

exec dnsmasq \
  --no-daemon \
  --interface="$IF" \
  --bind-interfaces \
  --except-interface=lo \
  --dhcp-range="${RANGE_START},${RANGE_END},255.255.255.0,12h" \
  --dhcp-option=option:router,"${GATEWAY}" \
  --dhcp-option=option:dns-server,"${GATEWAY}" \
  --log-dhcp
