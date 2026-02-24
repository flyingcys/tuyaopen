#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LLM_C="${APP_DIR}/llm/llm_proxy.c"

if [[ ! -f "${LLM_C}" ]]; then
  echo "FAIL: ${LLM_C} not found" >&2
  exit 1
fi

if ! grep -q "LLM_API_KEY_MAX_LEN" "${LLM_C}"; then
  echo "FAIL: enlarged API key buffer define is missing" >&2
  exit 1
fi

if ! grep -q "max_completion_tokens" "${LLM_C}"; then
  echo "FAIL: OpenAI max_completion_tokens field is missing" >&2
  exit 1
fi

echo "PASS: LLM OpenAI request compatibility updates are present."
