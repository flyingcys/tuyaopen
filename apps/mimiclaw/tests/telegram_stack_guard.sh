#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TG_C="${APP_DIR}/telegram/telegram_bot.c"

if [[ ! -f "${TG_C}" ]]; then
  echo "FAIL: ${TG_C} not found" >&2
  exit 1
fi

if grep -q "char resp\\[TG_HTTP_RESP_BUF_SIZE\\]" "${TG_C}"; then
  echo "FAIL: telegram_bot.c still allocates TG_HTTP_RESP_BUF_SIZE on thread stack" >&2
  exit 1
fi

if ! grep -q "tal_malloc(TG_HTTP_RESP_BUF_SIZE)" "${TG_C}"; then
  echo "FAIL: telegram_bot.c does not allocate response buffer from heap" >&2
  exit 1
fi

echo "PASS: telegram response buffer is heap-based (no large stack array)."
