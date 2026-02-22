#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TG_C="${APP_DIR}/telegram/telegram_bot.c"

if [[ ! -f "${TG_C}" ]]; then
  echo "FAIL: ${TG_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx text chat=' "${TG_C}"; then
  echo "FAIL: telegram text inbound log is missing" >&2
  exit 1
fi

if ! grep -q 'rx document chat=' "${TG_C}"; then
  echo "FAIL: telegram document inbound log is missing" >&2
  exit 1
fi

echo "PASS: telegram inbound text/document logs are present."
