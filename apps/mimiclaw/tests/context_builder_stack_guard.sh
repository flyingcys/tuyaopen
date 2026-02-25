#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CTX_C="${APP_DIR}/agent/context_builder.c"

if [[ ! -f "${CTX_C}" ]]; then
  echo "FAIL: ${CTX_C} not found" >&2
  exit 1
fi

if grep -q "char mem_buf\\[4096\\]" "${CTX_C}"; then
  echo "FAIL: context_builder.c still allocates mem_buf[4096] on stack" >&2
  exit 1
fi

if grep -q "char recent_buf\\[4096\\]" "${CTX_C}"; then
  echo "FAIL: context_builder.c still allocates recent_buf[4096] on stack" >&2
  exit 1
fi

if grep -q "char skills_buf\\[2048\\]" "${CTX_C}"; then
  echo "FAIL: context_builder.c still allocates skills_buf[2048] on stack" >&2
  exit 1
fi

if ! grep -q "tal_malloc(" "${CTX_C}"; then
  echo "FAIL: context_builder.c missing heap allocation for temporary buffers" >&2
  exit 1
fi

echo "PASS: context_builder avoids large stack buffers."
