#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ ! -d "${APP_DIR}" ]]; then
  echo "FAIL: app dir not found: ${APP_DIR}" >&2
  exit 1
fi

fail=0

check_forbidden() {
  local pattern="$1"
  local description="$2"
  local tmp
  tmp="$(mktemp)"
  if rg -n -i "${pattern}" "${APP_DIR}" --glob '!tests/**' >"${tmp}"; then
    echo "FAIL: found forbidden ${description} in logs" >&2
    cat "${tmp}" >&2
    fail=1
  fi
  rm -f "${tmp}"
}

check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*token[^"]*"' 'credential keyword output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*key[^"]*"' 'credential keyword output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*secret[^"]*"' 'credential keyword output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*password[^"]*"' 'credential keyword output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*authorization[^"]*"' 'credential keyword output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*bearer[^"]*"' 'credential keyword output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*url\\s*=\\s*%[^"]*"' 'URL output'
check_forbidden 'MIMI_LOG[EDIW]\([^,]*,[[:space:]]*"[^"]*(raw response|resp=%\\.[0-9]+s|text=%s|content=%\\.[0-9]+s)[^"]*"' 'raw payload/content output'

if [[ "${fail}" -ne 0 ]]; then
  exit 1
fi

echo "PASS: no sensitive credential/url/payload log output patterns found."
