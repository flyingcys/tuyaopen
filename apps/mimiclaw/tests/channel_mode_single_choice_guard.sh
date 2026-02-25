#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"
MIMI_C="${APP_DIR}/mimi.c"
SECRETS_EXAMPLE="${APP_DIR}/mimi_secrets.h.example"

for f in "${CLI_C}" "${MIMI_C}" "${SECRETS_EXAMPLE}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing required file: ${f}" >&2
    exit 1
  fi
done

if grep -q 'auto|telegram|discord|feishu|both' "${CLI_C}"; then
  echo "FAIL: legacy channel_mode options auto/both still exposed in CLI" >&2
  exit 1
fi

if ! grep -q 'telegram|discord|feishu' "${CLI_C}"; then
  echo "FAIL: CLI is missing strict 3-choice channel_mode options" >&2
  exit 1
fi

if ! grep -q 'invalid mode: %s (use telegram|discord|feishu)' "${CLI_C}"; then
  echo "FAIL: invalid-mode hint still allows legacy options" >&2
  exit 1
fi

if grep -q 'MIMI_CHANNEL_MODE_AUTO' "${MIMI_C}" || grep -q 'MIMI_CHANNEL_MODE_BOTH' "${MIMI_C}"; then
  echo "FAIL: runtime still keeps AUTO/BOTH channel mode states" >&2
  exit 1
fi

if ! grep -q 'legacy channel_mode=%s mapped to telegram' "${MIMI_C}"; then
  echo "FAIL: legacy auto/both migration to telegram is missing" >&2
  exit 1
fi

if ! grep -q 'Channel Mode: telegram | discord | feishu' "${SECRETS_EXAMPLE}"; then
  echo "FAIL: secrets example still documents legacy channel_mode values" >&2
  exit 1
fi

if ! grep -q '#define MIMI_SECRET_CHANNEL_MODE    "telegram"' "${SECRETS_EXAMPLE}"; then
  echo "FAIL: secrets example default channel_mode is not telegram" >&2
  exit 1
fi

echo "PASS: channel_mode is strict single-choice with telegram legacy fallback."
