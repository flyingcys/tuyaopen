#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TG_C="${APP_DIR}/telegram/telegram_bot.c"

if [[ ! -f "${TG_C}" ]]; then
  echo "FAIL: ${TG_C} not found" >&2
  exit 1
fi

if ! grep -q "TG_OFFSET_NVS_KEY" "${TG_C}"; then
  echo "FAIL: telegram update offset storage key is missing" >&2
  exit 1
fi

if ! grep -q "save_update_offset_if_needed" "${TG_C}"; then
  echo "FAIL: telegram periodic offset persistence is missing" >&2
  exit 1
fi

if ! grep -q "s_seen_msg_keys" "${TG_C}"; then
  echo "FAIL: telegram duplicate-message cache is missing" >&2
  exit 1
fi

echo "PASS: telegram dedup + offset persistence hooks are present."
