#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
WIFI_C="${APP_DIR}/wifi/wifi_manager.c"

if [[ ! -f "${WIFI_C}" ]]; then
  echo "FAIL: ${WIFI_C} not found" >&2
  exit 1
fi

if ! grep -q "tal_wifi_station_connect" "${WIFI_C}"; then
  echo "FAIL: wifi_manager.c does not use tal_wifi_station_connect" >&2
  exit 1
fi

if ! grep -q "tal_wifi_set_work_mode(WWM_STATION)" "${WIFI_C}"; then
  echo "FAIL: wifi_manager.c does not set station mode before connect" >&2
  exit 1
fi

if grep -q "NETCONN_CMD_CLOSE" "${WIFI_C}"; then
  echo "FAIL: wifi_manager.c still contains NETCONN_CMD_CLOSE in WiFi connect path" >&2
  exit 1
fi

echo "PASS: wifi connect implementation matches STA example style."
