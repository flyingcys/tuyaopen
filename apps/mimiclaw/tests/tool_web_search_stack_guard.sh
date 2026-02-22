#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
SEARCH_C="${APP_DIR}/tools/tool_web_search.c"

if [[ ! -f "${SEARCH_C}" ]]; then
  echo "FAIL: ${SEARCH_C} not found" >&2
  exit 1
fi

if grep -q "char resp\\[SEARCH_RESP_BUF_SIZE\\]" "${SEARCH_C}"; then
  echo "FAIL: tool_web_search.c still allocates SEARCH_RESP_BUF_SIZE on thread stack" >&2
  exit 1
fi

if ! grep -q "tal_malloc(SEARCH_RESP_BUF_SIZE)" "${SEARCH_C}"; then
  echo "FAIL: tool_web_search.c does not allocate response buffer from heap" >&2
  exit 1
fi

echo "PASS: web_search response buffer is heap-based (no large stack array)."
