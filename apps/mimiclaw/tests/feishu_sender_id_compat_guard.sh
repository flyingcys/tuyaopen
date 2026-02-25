#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FS_C="${APP_DIR}/channels/feishu_bot.c"

if [[ ! -f "${FS_C}" ]]; then
  echo "FAIL: ${FS_C} not found" >&2
  exit 1
fi

if ! grep -q 'json_str2(sender_id_obj, "user_id", NULL)' "${FS_C}"; then
  echo "FAIL: feishu sender user_id fallback is missing" >&2
  exit 1
fi

if ! grep -q 'json_str2(sender_id_obj, "union_id", NULL)' "${FS_C}"; then
  echo "FAIL: feishu sender union_id fallback is missing" >&2
  exit 1
fi

if ! grep -q 'sender_allowed_token' "${FS_C}"; then
  echo "FAIL: allow_from matching does not support compound sender IDs" >&2
  exit 1
fi

if ! grep -q 'strip_optional_quotes' "${FS_C}"; then
  echo "FAIL: allow_from matching does not normalize quoted IDs" >&2
  exit 1
fi

echo "PASS: feishu sender allowlist supports open_id/user_id/union_id compatibility."
