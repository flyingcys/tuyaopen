#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TLS_C="${APP_DIR}/tls_cert_bundle.c"

if [[ ! -f "${TLS_C}" ]]; then
  echo "FAIL: ${TLS_C} not found" >&2
  exit 1
fi

if grep -q "api.deepseek.com" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still has host-specific fallback branch for api.deepseek.com" >&2
  exit 1
fi

if grep -q "DEEPSEEK_DIGICERT_G2_CA_PEM" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still has DeepSeek-specific built-in CA blob" >&2
  exit 1
fi

echo "PASS: DeepSeek-specific hardcoded TLS fallback was removed."
