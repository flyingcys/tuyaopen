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

if ! grep -q "typedef void (\\*session_list_cb_t)" "${SESSION_H}"; then
  echo "FAIL: session list callback type missing in session_mgr.h" >&2
  exit 1
fi

if ! grep -q "session_list(session_list_cb_t cb, void \\*user_data, uint32_t \\*out_count)" "${SESSION_H}"; then
  echo "FAIL: session_list callback API signature missing in session_mgr.h" >&2
  exit 1
fi

if ! grep -Fq 'strcmp(name, ".") == 0' "${SESSION_C}"; then
  echo "FAIL: session_list does not filter '.' entry" >&2
  exit 1
fi

if ! grep -Fq 'strcmp(name, "..") == 0' "${SESSION_C}"; then
  echo "FAIL: session_list does not filter '..' entry" >&2
  exit 1
fi

if ! grep -q "session_list(cli_session_list_cb, &ctx, &count)" "${CLI_C}"; then
  echo "FAIL: cli session_list does not consume callback API" >&2
  exit 1
fi

if ! grep -Fq 'cli_echof("session[%u]: %s"' "${CLI_C}"; then
  echo "FAIL: cli session_list output format missing" >&2
  exit 1
fi

echo "PASS: session_list CLI callback and dot-entry filter are present."
