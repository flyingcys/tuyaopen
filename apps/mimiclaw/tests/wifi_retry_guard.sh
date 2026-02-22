#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
WIFI_C="${APP_DIR}/wifi/wifi_manager.c"

if [[ ! -f "${WIFI_C}" ]]; then
  echo "FAIL: ${WIFI_C} not found" >&2
  exit 1
fi

if ! grep -q "MIMI_WIFI_MAX_RETRY" "${WIFI_C}"; then
  echo "FAIL: wifi_manager.c does not apply MIMI_WIFI_MAX_RETRY" >&2
  exit 1
fi

if ! grep -q "schedule wifi retry" "${WIFI_C}"; then
  echo "FAIL: wifi_manager.c has no retry scheduling log on connect failure" >&2
  exit 1
fi

if ! grep -q "retrying wifi connect attempt" "${WIFI_C}"; then
  echo "FAIL: wifi_manager.c has no active reconnect attempt in wait loop" >&2
  exit 1
fi

echo "PASS: wifi retry scheduling and reconnect attempts are implemented."
