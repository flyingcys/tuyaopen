#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TLS_C="${APP_DIR}/tls_cert_bundle.c"

if [[ ! -f "${TLS_C}" ]]; then
  echo "FAIL: ${TLS_C} not found" >&2
  exit 1
fi

if ! grep -q "MIMI_TLS_CERT_QUERY_RETRY_COUNT" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no retry count config" >&2
  exit 1
fi

if ! grep -q "iotdns cert query retry" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no retry log for iotdns failures" >&2
  exit 1
fi

if ! grep -q "tal_system_sleep" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no retry backoff sleep" >&2
  exit 1
fi

echo "PASS: tls cert query retry/backoff is implemented."
