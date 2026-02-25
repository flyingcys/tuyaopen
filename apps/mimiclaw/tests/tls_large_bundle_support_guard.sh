#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TLS_H="${APP_DIR}/tls_cert_bundle.h"
TLS_C="${APP_DIR}/tls_cert_bundle.c"
CFG_H="${APP_DIR}/mimi_config.h"

for f in "${TLS_H}" "${TLS_C}" "${CFG_H}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing file ${f}" >&2
    exit 1
  fi
done

if ! grep -q "mimi_tls_query_domain_certs(const char \*host_or_url, uint8_t \*\*cacert, size_t \*cacert_len)" "${TLS_H}"; then
  echo "FAIL: mimi_tls_query_domain_certs still not using size_t cert length" >&2
  exit 1
fi

if grep -q "UINT16_MAX" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still has UINT16_MAX hard limit" >&2
  exit 1
fi

if ! grep -q "MIMI_TLS_GLOBAL_CA_BUNDLE_MAX_BYTES" "${CFG_H}"; then
  echo "FAIL: missing configurable MIMI_TLS_GLOBAL_CA_BUNDLE_MAX_BYTES" >&2
  exit 1
fi

echo "PASS: large global CA bundle support wiring exists (size_t path, no uint16 hard cap)."
