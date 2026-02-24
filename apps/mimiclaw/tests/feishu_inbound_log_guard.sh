#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FS_C="${APP_DIR}/channels/feishu_bot.c"

if [[ ! -f "${FS_C}" ]]; then
  echo "FAIL: ${FS_C} not found" >&2
  exit 1
fi

if ! grep -q 'rx feishu event=%s chat=%s sender=%s type=%s len=%u text=%s' "${FS_C}"; then
  echo "FAIL: feishu inbound log does not include message content" >&2
  exit 1
fi

echo "PASS: feishu inbound log includes message content."
