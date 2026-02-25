#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FS_C="${APP_DIR}/channels/feishu_bot.c"

if [[ ! -f "${FS_C}" ]]; then
  echo "FAIL: ${FS_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx inbound_text channel=%s chat=%s event=%s type=%s len=%u' "${FS_C}"; then
  echo "FAIL: feishu sanitized inbound log is missing key fields" >&2
  exit 1
fi

if grep -q 'rx inbound_text channel=%s chat=%s len=%u text=%s' "${FS_C}"; then
  echo "FAIL: feishu inbound log still prints raw text" >&2
  exit 1
fi

if grep -q 'content=%.512s' "${FS_C}"; then
  echo "FAIL: feishu raw inbound content preview log still exists" >&2
  exit 1
fi

echo "PASS: feishu inbound logs are sanitized (no raw text/content preview)."
