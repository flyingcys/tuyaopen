#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CFG_H="${APP_DIR}/mimi_config.h"
TLS_C="${APP_DIR}/tls_cert_bundle.c"

for f in "${CFG_H}" "${TLS_C}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing file ${f}" >&2
    exit 1
  fi
done

if ! grep -q "MIMI_TLS_GLOBAL_CA_ENABLE" "${CFG_H}"; then
  echo "FAIL: missing MIMI_TLS_GLOBAL_CA_ENABLE in mimi_config.h" >&2
  exit 1
fi

if ! grep -q "MIMI_TLS_GLOBAL_CA_BUNDLE_PATH" "${CFG_H}"; then
  echo "FAIL: missing MIMI_TLS_GLOBAL_CA_BUNDLE_PATH in mimi_config.h" >&2
  exit 1
fi

if ! grep -q "MIMI_TLS_GLOBAL_CA_BUNDLE_URL" "${CFG_H}"; then
  echo "FAIL: missing MIMI_TLS_GLOBAL_CA_BUNDLE_URL in mimi_config.h" >&2
  exit 1
fi

if ! grep -q "mimi_tls_load_global_ca_bundle" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no global ca bundle loader" >&2
  exit 1
fi

if ! grep -q "tal_fopen" "${TLS_C}" || ! grep -q "tal_fgetsize" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c does not load global ca bundle from file" >&2
  exit 1
fi

if ! grep -q "fallback to global ca bundle" "${TLS_C}"; then
  echo "FAIL: no iotdns->global ca fallback log found in tls_cert_bundle.c" >&2
  exit 1
fi

if ! grep -q "mimi_tls_download_global_ca_bundle" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no global ca bundle download path" >&2
  exit 1
fi

if ! grep -q "downloaded global ca bundle" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no download success log for global ca bundle" >&2
  exit 1
fi

if ! grep -q "skip iotdns host=.*due to fail cache" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no iotdns fail-cache skip log" >&2
  exit 1
fi

if ! grep -q "MIMI_TLS_CERT_FAIL_RETRY_INTERVAL_MS" "${CFG_H}"; then
  echo "FAIL: missing fail-cache retry interval config in mimi_config.h" >&2
  exit 1
fi

echo "PASS: global CA bundle fallback wiring exists for iotdns failure path."
