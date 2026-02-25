#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DC_C="${APP_DIR}/channels/discord_bot.c"

if [[ ! -f "${DC_C}" ]]; then
  echo "FAIL: ${DC_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx inbound_text channel=%s chat=%s len=%u' "${DC_C}"; then
  echo "FAIL: discord sanitized inbound text log is missing" >&2
  exit 1
fi

if grep -q 'rx inbound_text channel=%s chat=%s len=%u text=%s' "${DC_C}"; then
  echo "FAIL: discord inbound text log still prints raw text" >&2
  exit 1
fi

if ! grep -q 'rx attachment chat=%s message_id=%s name=%s mime=%s size=%u' "${DC_C}"; then
  echo "FAIL: discord sanitized attachment log is missing" >&2
  exit 1
fi

if grep -q 'url=%s' "${DC_C}"; then
  echo "FAIL: discord attachment log still prints URL" >&2
  exit 1
fi

echo "PASS: discord inbound logs are sanitized (no raw text/url)."
