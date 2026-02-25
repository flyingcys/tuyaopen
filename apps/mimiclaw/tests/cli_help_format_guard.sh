#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"

if [[ ! -f "${CLI_C}" ]]; then
  echo "FAIL: ${CLI_C} not found" >&2
  exit 1
fi

if ! grep -Fq '.name = "file_list"' "${CLI_C}"; then
  echo "FAIL: file_list missing from CLI source" >&2
  exit 1
fi

if ! grep -Fq '.name = "file_list", .help = "List /spiffs entries", .func = cmd_file_list' "${CLI_C}"; then
  echo "FAIL: file_list not registered in command table" >&2
  exit 1
fi

if ! grep -Fq '.arg_1 = " <open_id_csv>  Comma-separated sender open_id allowlist"' "${CLI_C}"; then
  echo "FAIL: set_fs_allow argument indentation is not aligned" >&2
  exit 1
fi

echo "PASS: file_list is exposed and set_fs_allow help indentation is aligned."
