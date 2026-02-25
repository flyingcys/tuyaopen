#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"
SESSION_C="${APP_DIR}/memory/session_mgr.c"
SESSION_H="${APP_DIR}/memory/session_mgr.h"

for f in "${CLI_C}" "${SESSION_C}" "${SESSION_H}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: ${f} not found" >&2
    exit 1
  fi
done

if ! grep -q "OPERATE_RET session_clear_all(uint32_t \\*out_removed)" "${SESSION_H}"; then
  echo "FAIL: session_clear_all API missing in session_mgr.h" >&2
  exit 1
fi

if ! grep -q "session_clear_all(&removed)" "${CLI_C}"; then
  echo "FAIL: cli command does not call session_clear_all" >&2
  exit 1
fi

if ! grep -q '\.name = "session_clear_all"' "${CLI_C}"; then
  echo "FAIL: session_clear_all command missing in cli registry" >&2
  exit 1
fi

if ! grep -q '\.name = "file_clear"' "${CLI_C}"; then
  echo "FAIL: file_clear command missing in cli registry" >&2
  exit 1
fi

if ! grep -Fq 'usage: file_clear <path>' "${CLI_C}"; then
  echo "FAIL: file_clear usage is missing" >&2
  exit 1
fi

if ! grep -Fq 'strncmp(path, MIMI_SPIFFS_BASE "/", sizeof(MIMI_SPIFFS_BASE)) != 0' "${CLI_C}"; then
  echo "FAIL: file_clear does not restrict path to /spiffs/" >&2
  exit 1
fi

if ! grep -Fq 'tal_fs_remove(path)' "${CLI_C}"; then
  echo "FAIL: file_clear does not remove target file by path" >&2
  exit 1
fi

if ! grep -Fq 'strncmp(chat_id, "tg_", 3) == 0' "${SESSION_C}"; then
  echo "FAIL: session_clear does not normalize tg_*.jsonl input" >&2
  exit 1
fi

if ! grep -Fq 'strstr(start, ".jsonl")' "${SESSION_C}"; then
  echo "FAIL: session_clear does not strip .jsonl suffix" >&2
  exit 1
fi

if ! grep -Fq "strchr(chat_id, '/')" "${SESSION_C}"; then
  echo "FAIL: session_clear should reject path-style input" >&2
  exit 1
fi

echo "PASS: session cleanup APIs and CLI command wiring are present."
