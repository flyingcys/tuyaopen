#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUS_H="${APP_DIR}/bus/message_bus.h"
MIMI_C="${APP_DIR}/mimi.c"
CLI_C="${APP_DIR}/cli/serial_cli.c"
CFG_H="${APP_DIR}/mimi_config.h"
SECRETS_EXAMPLE="${APP_DIR}/mimi_secrets.h.example"
CMAKE_FILE="${APP_DIR}/CMakeLists.txt"

for f in "${BUS_H}" "${MIMI_C}" "${CLI_C}" "${CFG_H}" "${SECRETS_EXAMPLE}" "${CMAKE_FILE}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing required file: ${f}" >&2
    exit 1
  fi
done

if ! grep -q 'MIMI_CHAN_DISCORD' "${BUS_H}"; then
  echo "FAIL: discord channel constant is missing from message bus" >&2
  exit 1
fi

if ! grep -q 'channels/discord_bot.h' "${MIMI_C}"; then
  echo "FAIL: mimi.c does not include discord bot header" >&2
  exit 1
fi

if ! grep -q 'strcmp(msg.channel, MIMI_CHAN_DISCORD)' "${MIMI_C}"; then
  echo "FAIL: outbound dispatch does not handle discord channel" >&2
  exit 1
fi

if ! grep -q 'discord_bot_init' "${MIMI_C}" || ! grep -q 'discord_bot_start' "${MIMI_C}"; then
  echo "FAIL: discord bot is not initialized and started from mimi.c" >&2
  exit 1
fi

if ! grep -q 'channels/discord_bot.c' "${CMAKE_FILE}"; then
  echo "FAIL: discord source file is not in CMakeLists.txt" >&2
  exit 1
fi

if ! grep -q 'set_dc_token' "${CLI_C}"; then
  echo "FAIL: CLI command set_dc_token is missing" >&2
  exit 1
fi

if ! grep -q 'set_dc_channel' "${CLI_C}"; then
  echo "FAIL: CLI command set_dc_channel is missing" >&2
  exit 1
fi

if ! grep -q 'set_channel_mode' "${CLI_C}"; then
  echo "FAIL: CLI command set_channel_mode is missing" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_DC_TOKEN' "${CFG_H}"; then
  echo "FAIL: MIMI_SECRET_DC_TOKEN is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_DC_CHANNEL_ID' "${CFG_H}"; then
  echo "FAIL: MIMI_SECRET_DC_CHANNEL_ID is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_CHANNEL_MODE' "${CFG_H}"; then
  echo "FAIL: MIMI_SECRET_CHANNEL_MODE is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_NVS_KEY_DC_TOKEN' "${CFG_H}" || ! grep -q 'MIMI_NVS_KEY_DC_CHANNEL_ID' "${CFG_H}"; then
  echo "FAIL: discord NVS keys are missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_NVS_KEY_CHANNEL_MODE' "${CFG_H}"; then
  echo "FAIL: channel mode NVS key is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_DC_TOKEN' "${SECRETS_EXAMPLE}" || ! grep -q 'MIMI_SECRET_DC_CHANNEL_ID' "${SECRETS_EXAMPLE}"; then
  echo "FAIL: discord secrets are missing from mimi_secrets.h.example" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_CHANNEL_MODE' "${SECRETS_EXAMPLE}"; then
  echo "FAIL: channel mode secret is missing from mimi_secrets.h.example" >&2
  exit 1
fi

echo "PASS: discord integration wiring is present."
