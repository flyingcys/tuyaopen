#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FS_C="${APP_DIR}/channels/feishu_bot.c"

if [[ ! -f "${FS_C}" ]]; then
  echo "FAIL: ${FS_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx feishu inbound chat=%s sender=%s event=%s message_id=%s type=%s chat_type=%s len=%u text=%s' "${FS_C}"; then
  echo "FAIL: feishu structured inbound log is missing key fields" >&2
  exit 1
fi

if ! grep -q 'rx inbound_text channel=%s chat=%s len=%u text=%s' "${FS_C}"; then
  echo "FAIL: feishu unified inbound text log is missing" >&2
  exit 1
fi

if ! grep -q 'rx feishu raw message_id=%s content=%.512s' "${FS_C}"; then
  echo "FAIL: feishu raw inbound content preview log is missing" >&2
  exit 1
fi

echo "PASS: feishu inbound logs include structured and raw content previews."
