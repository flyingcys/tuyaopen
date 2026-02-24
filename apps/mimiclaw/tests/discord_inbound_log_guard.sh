#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DC_C="${APP_DIR}/channels/discord_bot.c"

if [[ ! -f "${DC_C}" ]]; then
  echo "FAIL: ${DC_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx discord event=MESSAGE_CREATE channel=%s id=%s len=%u text=%s' "${DC_C}"; then
  echo "FAIL: discord inbound log does not include message content" >&2
  exit 1
fi

echo "PASS: discord inbound log includes message content."
