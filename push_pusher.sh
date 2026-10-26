#!/usr/bin/env bash
# Trigger Pusher event from host (reads ./.env). Not used by ESP32 firmware.
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

CHANNEL="${PUSHER_CHANNEL:-my-channel}"
KEY="${KEY//\"/}"
SECRET="${SECRET//\"/}"
CLUSTER="${CLUSTER//\"/}"
APP_ID="${APP_ID//\"/}"

BODY='{"name":"my-event","channel":"'"$CHANNEL"'","data":"{\"cmd\":\"press,power,short\"}"}'
BODY_MD5=$(echo -n "$BODY" | md5sum | awk '{print $1}')
TIMESTAMP=$(date +%s)
QUERY_STRING="auth_key=$KEY&auth_timestamp=$TIMESTAMP&auth_version=1.0&body_md5=$BODY_MD5"
printf -v STRING_TO_SIGN "POST\n/apps/$APP_ID/events\n$QUERY_STRING"
SIGNATURE=$(printf "%s" "$STRING_TO_SIGN" | openssl dgst -sha256 -hmac "$SECRET" | awk '{print $2}')

curl -fsS -X POST -H "Content-Type: application/json" \
  -d "$BODY" \
  "https://api-$CLUSTER.pusher.com/apps/$APP_ID/events?$QUERY_STRING&auth_signature=$SIGNATURE"
