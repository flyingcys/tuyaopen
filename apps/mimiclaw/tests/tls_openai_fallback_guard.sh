#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TLS_C="${APP_DIR}/tls_cert_bundle.c"

if [[ ! -f "${TLS_C}" ]]; then
  echo "FAIL: ${TLS_C} not found" >&2
  exit 1
fi

if ! grep -q "api.openai.com" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no built-in fallback branch for api.openai.com" >&2
  exit 1
fi

if ! grep -q "OPENAI_WE1_CA_PEM" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no OpenAI WE1 fallback CA blob" >&2
  exit 1
fi

if grep -q "OPENAI_GTS_R4_CA_PEM" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still uses OpenAI GTS Root R4 fallback (P-384 unsupported on target)" >&2
  exit 1
fi

echo "PASS: OpenAI built-in CA fallback is implemented."
