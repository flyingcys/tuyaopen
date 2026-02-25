#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"

if [[ ! -f "${CLI_C}" ]]; then
  echo "FAIL: ${CLI_C} not found" >&2
  exit 1
fi

if ! grep -Fq 'usage: memory_read' "${CLI_C}"; then
  echo "FAIL: memory_read usage hint missing" >&2
  exit 1
fi

if grep -Fq 'usage: memory_read [path]' "${CLI_C}"; then
  echo "FAIL: memory_read should no longer accept optional path" >&2
  exit 1
fi

if ! grep -Fq 'memory_read_long_term(buf, 4096)' "${CLI_C}"; then
  echo "FAIL: memory_read should use memory_read_long_term only" >&2
  exit 1
fi

if ! grep -Fq '.name = "file_read"' "${CLI_C}"; then
  echo "FAIL: file_read command is not registered" >&2
  exit 1
fi

if ! grep -Fq 'usage: file_read <path>' "${CLI_C}"; then
  echo "FAIL: file_read usage is missing" >&2
  exit 1
fi

if ! grep -Fq 'strncmp(path, MIMI_SPIFFS_BASE "/", sizeof(MIMI_SPIFFS_BASE)) != 0' "${CLI_C}"; then
  echo "FAIL: file_read does not restrict path to /spiffs" >&2
  exit 1
fi

if ! grep -Fq 'TUYA_FILE f = tal_fopen(path, "r")' "${CLI_C}"; then
  echo "FAIL: file_read does not open target file by path" >&2
  exit 1
fi

if ! grep -Fq 'int n = tal_fread(buf, 4095, f);' "${CLI_C}"; then
  echo "FAIL: file_read does not read file content into CLI buffer" >&2
  exit 1
fi

echo "PASS: memory_read scope is restored and file_read is available."
