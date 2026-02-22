#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LLM_C="${APP_DIR}/llm/llm_proxy.c"

if [[ ! -f "${LLM_C}" ]]; then
  echo "FAIL: ${LLM_C} not found" >&2
  exit 1
fi

if grep -q "char raw_resp\\[MIMI_LLM_STREAM_BUF_SIZE\\]" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c still allocates MIMI_LLM_STREAM_BUF_SIZE on thread stack" >&2
  exit 1
fi

if ! grep -q "calloc(1, MIMI_LLM_STREAM_BUF_SIZE)" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c does not allocate raw response buffer from heap" >&2
  exit 1
fi

echo "PASS: llm raw response buffer is heap-based (no large stack array)."
