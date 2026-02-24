#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"

if [[ ! -f "${CLI_C}" ]]; then
  echo "FAIL: ${CLI_C} not found" >&2
  exit 1
fi

if ! grep -q '\.name = "set_wifi"' "${CLI_C}"; then
  echo "FAIL: set_wifi command is missing in serial CLI" >&2
  exit 1
fi

if ! grep -q 'usage: set_wifi <ssid> <password>' "${CLI_C}"; then
  echo "FAIL: set_wifi usage/help is missing" >&2
  exit 1
fi

echo "PASS: set_wifi command is present."
