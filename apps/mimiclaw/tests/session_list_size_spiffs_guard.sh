#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"

if [[ ! -f "${CLI_C}" ]]; then
  echo "FAIL: ${CLI_C} not found" >&2
  exit 1
fi

if grep -Fq 'usage: session_list [all]' "${CLI_C}"; then
  echo "FAIL: session_list should not support [all]" >&2
  exit 1
fi

if grep -Fq 'strcmp(argv[1], "all") == 0' "${CLI_C}"; then
  echo "FAIL: session_list should not handle all mode" >&2
  exit 1
fi

if ! grep -Fq 'tal_fgetsize(path)' "${CLI_C}"; then
  echo "FAIL: session_list output does not include file size lookup" >&2
  exit 1
fi

if ! grep -Fq 'cli_echof("session[%u]: %s (%d B)"' "${CLI_C}"; then
  echo "FAIL: session_list output format does not include size" >&2
  exit 1
fi

if ! grep -Fq '.name = "file_list"' "${CLI_C}"; then
  echo "FAIL: file_list command is not registered" >&2
  exit 1
fi

if ! grep -Fq 'usage: file_list' "${CLI_C}"; then
  echo "FAIL: file_list usage is missing" >&2
  exit 1
fi

if ! grep -Fq 'cli_echof("/spiffs' "${CLI_C}"; then
  echo "FAIL: file_list does not print /spiffs entries" >&2
  exit 1
fi

echo "PASS: session_list is session-only and file_list handles /spiffs listing."
