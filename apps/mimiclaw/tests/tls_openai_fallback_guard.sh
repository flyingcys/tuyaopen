#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TLS_C="${APP_DIR}/tls_cert_bundle.c"

if [[ ! -f "${TLS_C}" ]]; then
  echo "FAIL: ${TLS_C} not found" >&2
  exit 1
fi

if grep -q "api.openai.com" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still has host-specific fallback branch for api.openai.com" >&2
  exit 1
fi

if grep -q "OPENAI_WE1_CA_PEM" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still has OpenAI-specific built-in CA blob" >&2
  exit 1
fi

if grep -q "OPENAI_GTS_R4_CA_PEM" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still has deprecated OpenAI GTS Root R4 symbol" >&2
  exit 1
fi

echo "PASS: OpenAI-specific hardcoded TLS fallback was removed."
