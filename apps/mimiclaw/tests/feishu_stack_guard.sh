#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
FS_C="${APP_DIR}/channels/feishu_bot.c"

if [[ ! -f "${FS_C}" ]]; then
  echo "FAIL: ${FS_C} not found" >&2
  exit 1
fi

if grep -q "fs_ws_conn_t conn;" "${FS_C}"; then
  echo "FAIL: feishu_bot.c still allocates fs_ws_conn_t on thread stack" >&2
  exit 1
fi

if grep -q "fs_pb_frame_t pb;" "${FS_C}"; then
  echo "FAIL: feishu_bot.c still allocates ws pb frame on thread stack" >&2
  exit 1
fi

if grep -q "fs_pb_frame_t ack;" "${FS_C}"; then
  echo "FAIL: feishu_bot.c still allocates ack pb frame on thread stack" >&2
  exit 1
fi

if grep -q "fs_pb_frame_t ping;" "${FS_C}"; then
  echo "FAIL: feishu_bot.c still allocates ping pb frame on thread stack" >&2
  exit 1
fi

if ! grep -q "sizeof(fs_ws_conn_t)" "${FS_C}"; then
  echo "FAIL: feishu_bot.c has no heap allocation path for fs_ws_conn_t" >&2
  exit 1
fi

if ! grep -q "sizeof(fs_pb_frame_t)" "${FS_C}"; then
  echo "FAIL: feishu_bot.c has no heap allocation path for fs_pb_frame_t" >&2
  exit 1
fi

echo "PASS: feishu websocket path avoids large stack allocations."
