#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TG_C="${APP_DIR}/channels/telegram_bot.c"

if [[ ! -f "${TG_C}" ]]; then
  echo "FAIL: ${TG_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx inbound_text channel=%s chat=%s len=%u' "${TG_C}"; then
  echo "FAIL: telegram sanitized inbound text log is missing" >&2
  exit 1
fi

if grep -q 'rx inbound_text channel=%s chat=%s len=%u text=%s' "${TG_C}"; then
  echo "FAIL: telegram inbound text log still prints raw text" >&2
  exit 1
fi

if ! grep -q 'rx document chat=%s name=%s mime=%s size=%u' "${TG_C}"; then
  echo "FAIL: telegram sanitized document log is missing" >&2
  exit 1
fi

if grep -q 'file_id=%s caption=%s' "${TG_C}"; then
  echo "FAIL: telegram document log still prints file_id/caption" >&2
  exit 1
fi

echo "PASS: telegram inbound logs are sanitized (no raw text/file_id/caption)."
