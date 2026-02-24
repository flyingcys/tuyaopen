#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
LLM_C="${APP_DIR}/llm/llm_proxy.c"

if [[ ! -f "${LLM_C}" ]]; then
  echo "FAIL: ${LLM_C} not found" >&2
  exit 1
fi

if grep -q "if (!cacert || !cacert_len || \\*cacert_len == 0)" "${LLM_C}"; then
  echo "FAIL: llm_proxy.c still blocks Linux no-verify fallback with strict cacert check" >&2
  exit 1
fi

echo "PASS: llm_proxy Linux no-verify fallback is not blocked by strict cacert check."
