#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"
WIFI_C="${APP_DIR}/wifi/wifi_manager.c"
WIFI_H="${APP_DIR}/wifi/wifi_manager.h"

for f in "${CLI_C}" "${WIFI_C}" "${WIFI_H}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: ${f} not found" >&2
    exit 1
  fi
done

if ! grep -q "wifi_manager_set_scan_result_cb" "${WIFI_H}"; then
  echo "FAIL: wifi scan result callback API missing in wifi_manager.h" >&2
  exit 1
fi

if ! grep -q "wifi_manager_set_scan_result_cb" "${CLI_C}"; then
  echo "FAIL: cli wifi_scan does not register scan callback" >&2
  exit 1
fi

if ! grep -Fq 'cli_echof("ap[%u] ssid=%s ch=%u rssi=%d sec=%u bssid=%s"' "${CLI_C}"; then
  echo "FAIL: cli wifi_scan callback output format is missing" >&2
  exit 1
fi

if ! grep -q "s_scan_result_cb" "${WIFI_C}"; then
  echo "FAIL: wifi_manager scan callback state is missing" >&2
  exit 1
fi

if ! grep -q "s_scan_result_cb(" "${WIFI_C}"; then
  echo "FAIL: wifi_manager scan callback is not invoked for AP entries" >&2
  exit 1
fi

echo "PASS: cli wifi_scan callback wiring is present."
