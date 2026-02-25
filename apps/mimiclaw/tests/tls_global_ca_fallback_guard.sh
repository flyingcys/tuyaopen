#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CFG_H="${APP_DIR}/mimi_config.h"
TLS_C="${APP_DIR}/tls_cert_bundle.c"
CERT_H="${APP_DIR}/certs/ca_bundle_mini.h"
CERT_C="${APP_DIR}/certs/ca_bundle_mini.c"
APP_CMAKE="${APP_DIR}/CMakeLists.txt"

for f in "${CFG_H}" "${TLS_C}" "${CERT_H}" "${CERT_C}" "${APP_CMAKE}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing file ${f}" >&2
    exit 1
  fi
done

if grep -q "MIMI_TLS_GLOBAL_CA_" "${CFG_H}"; then
  echo "FAIL: mimi_config.h still contains MIMI_TLS_GLOBAL_CA_* configs" >&2
  exit 1
fi

if grep -q "MIMI_TLS_GLOBAL_CA_" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still references MIMI_TLS_GLOBAL_CA_* path" >&2
  exit 1
fi

if grep -q "mimi_tls_download_global_ca_bundle" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still contains global CA download path" >&2
  exit 1
fi

if grep -q "tal_fopen\\|tal_fgetsize" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c still depends on SPIFFS CA bundle file I/O" >&2
  exit 1
fi

if ! grep -q "mimi_tls_load_builtin_ca_bundle" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no built-in CA bundle loader" >&2
  exit 1
fi

if ! grep -q "fallback to builtin ca bundle" "${TLS_C}"; then
  echo "FAIL: no iotdns->builtin ca fallback log found in tls_cert_bundle.c" >&2
  exit 1
fi

if ! grep -q "g_mimi_ca_bundle_mini_pem" "${CERT_H}" || ! grep -q "g_mimi_ca_bundle_mini_pem_len" "${CERT_H}"; then
  echo "FAIL: built-in mini CA symbols missing in ca_bundle_mini.h" >&2
  exit 1
fi

if ! grep -q "certs/ca_bundle_mini.c" "${APP_CMAKE}"; then
  echo "FAIL: CMakeLists.txt does not compile built-in mini CA source" >&2
  exit 1
fi

if ! grep -q "skip iotdns host=.*due to fail cache" "${TLS_C}"; then
  echo "FAIL: tls_cert_bundle.c has no iotdns fail-cache skip log" >&2
  exit 1
fi

if ! grep -q "MIMI_TLS_CERT_FAIL_CACHE_SLOTS" "${TLS_C}" || \
   ! grep -q "MIMI_TLS_CERT_FAIL_RETRY_INTERVAL_MS" "${TLS_C}" || \
   ! grep -q "MIMI_TLS_CERT_FAIL_LOG_INTERVAL_MS" "${TLS_C}"; then
  echo "FAIL: missing fail-cache fixed macros in tls_cert_bundle.c" >&2
  exit 1
fi

echo "PASS: built-in CA fallback wiring exists for iotdns failure path without SPIFFS/URL global CA."
