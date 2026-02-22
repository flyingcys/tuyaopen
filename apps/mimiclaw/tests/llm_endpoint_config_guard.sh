#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LLM_C="${APP_DIR}/llm/llm_proxy.c"

if [[ ! -f "${LLM_C}" ]]; then
  echo "FAIL: ${LLM_C} not found" >&2
  exit 1
fi

if ! grep -q "MIMI_OPENAI_API_URL" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c does not use MIMI_OPENAI_API_URL for endpoint selection" >&2
  exit 1
fi

if ! grep -q "MIMI_LLM_API_URL" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c does not use MIMI_LLM_API_URL for endpoint selection" >&2
  exit 1
fi

if grep -q "\"api.openai.com\"" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c still hardcodes api.openai.com" >&2
  exit 1
fi

if grep -q "\"api.anthropic.com\"" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c still hardcodes api.anthropic.com" >&2
  exit 1
fi

echo "PASS: llm endpoint host/path are config-driven."
