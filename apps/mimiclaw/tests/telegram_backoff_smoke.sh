#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
APP_DIR="${ROOT_DIR}/apps/mimiclaw"
BIN_PATH="${ROOT_DIR}/platform/LINUX/build/mimiclaw"
SECRETS_FILE="${APP_DIR}/mimi_secrets.h"
SECRETS_BAK="${SECRETS_FILE}.bak.codex"

cleanup() {
  rm -f "${RAW_LOG}" "${CLEAN_LOG}"
  if [[ -f "${SECRETS_BAK}" ]]; then
    mv -f "${SECRETS_BAK}" "${SECRETS_FILE}"
  else
    rm -f "${SECRETS_FILE}"
  fi
}

if [[ -f "${SECRETS_FILE}" ]]; then
  cp "${SECRETS_FILE}" "${SECRETS_BAK}"
fi

cat > "${SECRETS_FILE}" <<'EOF'
#pragma once
#define MIMI_SECRET_TG_TOKEN "dummy-token"
#define MIMI_TG_API_HOST "127.0.0.1"
#define MIMI_SECRET_API_KEY ""
#define MIMI_SECRET_MODEL ""
#define MIMI_SECRET_MODEL_PROVIDER "anthropic"
#define MIMI_SECRET_PROXY_HOST ""
#define MIMI_SECRET_PROXY_PORT ""
#define MIMI_SECRET_SEARCH_KEY ""
#define MIMI_SECRET_WIFI_SSID ""
#define MIMI_SECRET_WIFI_PASS ""
EOF

RAW_LOG="$(mktemp)"
CLEAN_LOG="$(mktemp)"
trap cleanup EXIT

(
  set +u
  source "${ROOT_DIR}/export.sh"
  touch "${APP_DIR}/channels/telegram_bot.c"
  cd "${APP_DIR}"
  tos.py build >/dev/null
)

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "ERROR: binary not found after build: ${BIN_PATH}" >&2
  exit 2
fi

set +e
timeout 14s "${BIN_PATH}" >"${RAW_LOG}" 2>&1
run_rc=$?
set -e

if [[ ${run_rc} -ne 0 && ${run_rc} -ne 124 ]]; then
  echo "ERROR: mimiclaw exited unexpectedly: rc=${run_rc}" >&2
  cat "${RAW_LOG}" >&2
  exit 1
fi

sed -E 's/\x1B\[[0-9;]*[[:alpha:]]//g' "${RAW_LOG}" >"${CLEAN_LOG}"

if ! grep -q "cert unavailable for 127.0.0.1" "${CLEAN_LOG}"; then
  echo "FAIL: telegram host override not applied (expected host=127.0.0.1 in cert fallback log)." >&2
  cat "${CLEAN_LOG}" >&2
  exit 1
fi

fail_count="$(grep -c "getUpdates failed" "${CLEAN_LOG}" || true)"
if [[ "${fail_count}" -gt 4 ]]; then
  echo "FAIL: getUpdates failure count too high within 14s: ${fail_count}" >&2
  echo "Expected <= 4 with exponential backoff." >&2
  cat "${CLEAN_LOG}" >&2
  exit 1
fi

echo "PASS: telegram polling failure backoff is effective."
