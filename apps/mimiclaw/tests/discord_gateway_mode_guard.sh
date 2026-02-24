#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DC_C="${APP_DIR}/channels/discord_bot.c"
CFG_H="${APP_DIR}/mimi_config.h"

if [[ ! -f "${DC_C}" || ! -f "${CFG_H}" ]]; then
  echo "FAIL: required files are missing" >&2
  exit 1
fi

if ! grep -q 'MIMI_DC_GATEWAY_HOST' "${CFG_H}"; then
  echo "FAIL: MIMI_DC_GATEWAY_HOST is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_DC_GATEWAY_PATH' "${CFG_H}"; then
  echo "FAIL: MIMI_DC_GATEWAY_PATH is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'discord gateway' "${DC_C}"; then
  echo "FAIL: discord gateway lifecycle logs are missing" >&2
  exit 1
fi

if ! grep -q 'MESSAGE_CREATE' "${DC_C}"; then
  echo "FAIL: MESSAGE_CREATE event handling is missing" >&2
  exit 1
fi

if ! grep -q '"op":2' "${DC_C}"; then
  echo "FAIL: Discord IDENTIFY payload (op=2) is missing" >&2
  exit 1
fi

if grep -q '/channels/%s/messages?limit=25' "${DC_C}"; then
  echo "FAIL: Discord inbound still uses REST polling instead of gateway events" >&2
  exit 1
fi

if grep -q 'http_client_request(' "${DC_C}"; then
  echo "FAIL: discord REST send still uses http_client_request (can downgrade to plain HTTP on 443)" >&2
  exit 1
fi

if grep -q "if (s_bot_token\\[0\\] == '\\\\0' || s_channel_id\\[0\\] == '\\\\0')" "${DC_C}"; then
  echo "FAIL: discord startup still requires channel_id for inbound" >&2
  exit 1
fi

echo "PASS: discord gateway mode wiring is present."
