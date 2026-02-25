#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DC_C="${APP_DIR}/channels/discord_bot.c"

if [[ ! -f "${DC_C}" ]]; then
  echo "FAIL: ${DC_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx text chat=' "${DC_C}"; then
  echo "FAIL: discord text inbound log is missing" >&2
  exit 1
fi

if ! grep -q 'rx inbound_text channel=%s chat=%s len=%u text=%s' "${DC_C}"; then
  echo "FAIL: discord unified inbound text log is missing" >&2
  exit 1
fi

if ! grep -q 'rx attachment chat=' "${DC_C}"; then
  echo "FAIL: discord attachment inbound log is missing" >&2
  exit 1
fi

echo "PASS: discord inbound text/attachment logs are present."
