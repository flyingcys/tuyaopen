#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROXY_H="${APP_DIR}/proxy/http_proxy.h"
PROXY_C="${APP_DIR}/proxy/http_proxy.c"
CLI_C="${APP_DIR}/cli/serial_cli.c"

for f in "${PROXY_H}" "${PROXY_C}" "${CLI_C}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: ${f} not found" >&2
    exit 1
  fi
done

if ! grep -q "http_proxy_set(const char \\*host, uint16_t port, const char \\*type)" "${PROXY_H}"; then
  echo "FAIL: http_proxy_set does not accept proxy type" >&2
  exit 1
fi

if ! grep -q "s_proxy_type" "${PROXY_C}"; then
  echo "FAIL: proxy type runtime state is missing" >&2
  exit 1
fi

if ! grep -q "socks5" "${PROXY_C}"; then
  echo "FAIL: socks5 path is missing in proxy implementation" >&2
  exit 1
fi

if ! grep -q "usage: set_proxy <host> <port> \\[type\\]" "${CLI_C}"; then
  echo "FAIL: CLI set_proxy type usage is missing" >&2
  exit 1
fi

echo "PASS: proxy type (http/socks5) plumbing is present."
