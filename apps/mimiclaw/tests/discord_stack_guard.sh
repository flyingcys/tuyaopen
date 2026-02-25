#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DC_C="${APP_DIR}/channels/discord_bot.c"

if [[ ! -f "${DC_C}" ]]; then
  echo "FAIL: ${DC_C} not found" >&2
  exit 1
fi

if grep -q "dc_gateway_conn_t conn;" "${DC_C}"; then
  echo "FAIL: discord_bot.c still allocates dc_gateway_conn_t on thread stack" >&2
  exit 1
fi

if grep -q "uint8_t rx_buf\\[MIMI_DC_GATEWAY_RX_BUF_SIZE\\]" "${DC_C}"; then
  echo "FAIL: discord gateway rx buffer is still embedded in dc_gateway_conn_t" >&2
  exit 1
fi

if ! grep -q "calloc(1, sizeof(dc_gateway_conn_t))" "${DC_C}"; then
  echo "FAIL: discord_bot.c missing heap allocation path for dc_gateway_conn_t" >&2
  exit 1
fi

if ! grep -q "tal_malloc(min_cap)" "${DC_C}" && ! grep -q "tal_malloc(MIMI_DC_GATEWAY_RX_BUF_SIZE)" "${DC_C}"; then
  echo "FAIL: discord_bot.c missing heap allocation for gateway rx buffer" >&2
  exit 1
fi

echo "PASS: discord gateway path avoids large stack allocations."
