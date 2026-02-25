#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"
LLM_C="${APP_DIR}/llm/llm_proxy.c"
LLM_H="${APP_DIR}/llm/llm_proxy.h"
CFG_H="${APP_DIR}/mimi_config.h"

for f in "${CLI_C}" "${LLM_C}" "${LLM_H}" "${CFG_H}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: ${f} not found" >&2
    exit 1
  fi
done

if ! grep -q 'MIMI_NVS_KEY_API_URL' "${CFG_H}"; then
  echo "FAIL: missing NVS key MIMI_NVS_KEY_API_URL in mimi_config.h" >&2
  exit 1
fi

if ! grep -q 'OPERATE_RET llm_set_api_url(const char \*api_url);' "${LLM_H}"; then
  echo "FAIL: llm_set_api_url declaration missing in llm_proxy.h" >&2
  exit 1
fi

if ! grep -q 'OPERATE_RET llm_set_api_url(const char \*api_url)' "${LLM_C}"; then
  echo "FAIL: llm_set_api_url definition missing in llm_proxy.c" >&2
  exit 1
fi

if ! grep -q 'mimi_kv_get_string(MIMI_NVS_LLM, MIMI_NVS_KEY_API_URL' "${LLM_C}"; then
  echo "FAIL: llm_proxy_init does not load API URL from NVS" >&2
  exit 1
fi

if ! grep -q 'MIMI_OPENAI_API_URL' "${LLM_C}" || ! grep -q 'MIMI_LLM_API_URL' "${LLM_C}"; then
  echo "FAIL: llm_proxy.c must retain config.h fallback URLs for openai/anthropic" >&2
  exit 1
fi

if ! grep -q 'set_api_url' "${CLI_C}"; then
  echo "FAIL: CLI command set_api_url missing" >&2
  exit 1
fi

if ! grep -q 'llm_set_api_url(argv\[1\])' "${CLI_C}"; then
  echo "FAIL: set_api_url command does not call llm_set_api_url" >&2
  exit 1
fi

echo "PASS: CLI API URL override wiring is present with NVS-first fallback logic."
