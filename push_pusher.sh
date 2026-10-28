#!/usr/bin/env bash
# Trigger Pusher event from host (reads ./.env). Not used by ESP32 firmware.
# Usage: ./push_pusher.sh [power|reset] [short|long]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
if [[ -f "$ROOT/.env" ]]; then
  set -a
  # shellcheck source=/dev/null
  source "$ROOT/.env"
  set +a
fi

: "${KEY:?KEY missing in .env}"
: "${SECRET:?SECRET missing in .env}"
: "${CLUSTER:?CLUSTER missing in .env}"
: "${APP_ID:?APP_ID missing in .env}"

TARGET="${1:-power}"
DUR="${2:-short}"
TARGET="${TARGET,,}"
DUR="${DUR,,}"
[[ "$TARGET" == power || "$TARGET" == reset ]] || {
  echo "Usage: $0 [power|reset] [short|long]"
  exit 1
}
[[ "$DUR" == short || "$DUR" == long ]] || {
  echo "Usage: $0 [power|reset] [short|long]"
  exit 1
}

CHANNEL="${PUSHER_CHANNEL:-my-channel}"
KEY="${KEY//\"/}"
SECRET="${SECRET//\"/}"
CLUSTER="${CLUSTER//\"/}"
APP_ID="${APP_ID//\"/}"

CMD="press,${TARGET},${DUR}"
BODY='{"name":"my-event","channel":"'"$CHANNEL"'","data":"{\"cmd\":\"'"$CMD"'\"}"}'
BODY_MD5=$(echo -n "$BODY" | md5sum | awk '{print $1}')
TIMESTAMP=$(date +%s)
QUERY_STRING="auth_key=$KEY&auth_timestamp=$TIMESTAMP&auth_version=1.0&body_md5=$BODY_MD5"
printf -v STRING_TO_SIGN "POST\n/apps/$APP_ID/events\n$QUERY_STRING"
SIGNATURE=$(printf "%s" "$STRING_TO_SIGN" | openssl dgst -sha256 -hmac "$SECRET" | awk '{print $2}')

echo "[pusher] trigger $CMD on channel $CHANNEL"
curl -fsS -X POST -H "Content-Type: application/json" \
  -d "$BODY" \
  "https://api-$CLUSTER.pusher.com/apps/$APP_ID/events?$QUERY_STRING&auth_signature=$SIGNATURE"
