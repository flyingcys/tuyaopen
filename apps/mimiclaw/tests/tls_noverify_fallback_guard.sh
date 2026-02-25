#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${APP_DIR}/../.." && pwd)"
HTTP_H="${ROOT_DIR}/src/libhttp/include/http_client_interface.h"
HTTP_C="${ROOT_DIR}/src/libhttp/src/http_client_wrapper.c"
TG_C="${APP_DIR}/channels/telegram_bot.c"
DC_C="${APP_DIR}/channels/discord_bot.c"

for f in "${HTTP_H}" "${HTTP_C}" "${TG_C}" "${DC_C}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing file ${f}" >&2
    exit 1
  fi
done

if ! grep -q "tls_no_verify" "${HTTP_H}"; then
  echo "FAIL: http_client_request_t has no tls_no_verify flag" >&2
  exit 1
fi

if ! grep -q "request->tls_no_verify" "${HTTP_C}"; then
  echo "FAIL: http_client_request does not handle tls_no_verify" >&2
  exit 1
fi

if ! grep -q "\.tls_no_verify = s_tg_tls_no_verify" "${TG_C}"; then
  echo "FAIL: telegram direct HTTP call does not pass tls_no_verify fallback" >&2
  exit 1
fi

if ! grep -q "fallback to TLS no-verify mode" "${DC_C}"; then
  echo "FAIL: discord direct TLS path has no no-verify fallback log" >&2
  exit 1
fi

echo "PASS: no-verify fallback wiring exists for iotdns failure path."
