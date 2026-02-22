#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TLS_C="${APP_DIR}/tls_cert_bundle.c"

if [[ ! -f "${TLS_C}" ]]; then
  echo "FAIL: ${TLS_C} not found" >&2
  exit 1
fi

if ! grep -q "api.deepseek.com" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no built-in fallback branch for api.deepseek.com" >&2
  exit 1
fi

if ! grep -q "DEEPSEEK_DIGICERT_G2_CA_PEM" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no DeepSeek fallback CA blob" >&2
  exit 1
fi

echo "PASS: DeepSeek built-in CA fallback is implemented."
