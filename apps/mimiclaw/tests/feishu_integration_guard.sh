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
FS_H="${APP_DIR}/channels/feishu_bot.h"
FS_C="${APP_DIR}/channels/feishu_bot.c"

for f in "${BUS_H}" "${MIMI_C}" "${CLI_C}" "${CFG_H}" "${SECRETS_EXAMPLE}" "${CMAKE_FILE}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing required file: ${f}" >&2
    exit 1
  fi
done

if ! grep -q 'MIMI_CHAN_FEISHU' "${BUS_H}"; then
  echo "FAIL: feishu channel constant is missing from message bus" >&2
  exit 1
fi

if ! grep -q 'channels/feishu_bot.h' "${MIMI_C}"; then
  echo "FAIL: mimi.c does not include feishu bot header" >&2
  exit 1
fi

if ! grep -q 'strcmp(msg.channel, MIMI_CHAN_FEISHU)' "${MIMI_C}"; then
  echo "FAIL: outbound dispatch does not handle feishu channel" >&2
  exit 1
fi

if ! grep -q 'feishu_bot_init' "${MIMI_C}" || ! grep -q 'feishu_bot_start' "${MIMI_C}"; then
  echo "FAIL: feishu bot is not initialized and started from mimi.c" >&2
  exit 1
fi

if ! grep -q 'channels/feishu_bot.c' "${CMAKE_FILE}"; then
  echo "FAIL: feishu source file is not in CMakeLists.txt" >&2
  exit 1
fi

if [[ ! -f "${FS_H}" || ! -f "${FS_C}" ]]; then
  echo "FAIL: feishu bot files are missing" >&2
  exit 1
fi

if ! grep -q 'set_fs_appid' "${CLI_C}"; then
  echo "FAIL: CLI command set_fs_appid is missing" >&2
  exit 1
fi

if ! grep -q 'set_fs_appsecret' "${CLI_C}"; then
  echo "FAIL: CLI command set_fs_appsecret is missing" >&2
  exit 1
fi

if ! grep -q 'set_fs_allow' "${CLI_C}"; then
  echo "FAIL: CLI command set_fs_allow is missing" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_FS_APP_ID' "${CFG_H}" || ! grep -q 'MIMI_SECRET_FS_APP_SECRET' "${CFG_H}"; then
  echo "FAIL: feishu secrets are missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_FS_ALLOW_FROM' "${CFG_H}"; then
  echo "FAIL: feishu allow_from secret is missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_NVS_FS' "${CFG_H}" || ! grep -q 'MIMI_NVS_KEY_FS_APP_ID' "${CFG_H}" || ! grep -q 'MIMI_NVS_KEY_FS_APP_SECRET' "${CFG_H}" || ! grep -q 'MIMI_NVS_KEY_FS_ALLOW_FROM' "${CFG_H}"; then
  echo "FAIL: feishu NVS keys are missing from mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'MIMI_SECRET_FS_APP_ID' "${SECRETS_EXAMPLE}" || ! grep -q 'MIMI_SECRET_FS_APP_SECRET' "${SECRETS_EXAMPLE}" || ! grep -q 'MIMI_SECRET_FS_ALLOW_FROM' "${SECRETS_EXAMPLE}"; then
  echo "FAIL: feishu secrets are missing from mimi_secrets.h.example" >&2
  exit 1
fi

if ! grep -q 'feishu' "${CLI_C}"; then
  echo "FAIL: CLI does not include feishu mode/config entries" >&2
  exit 1
fi

echo "PASS: feishu integration wiring is present."
