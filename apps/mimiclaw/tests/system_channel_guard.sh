#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUS_H="${APP_DIR}/bus/message_bus.h"
MIMI_C="${APP_DIR}/mimi.c"

if [[ ! -f "${BUS_H}" || ! -f "${MIMI_C}" ]]; then
  echo "FAIL: required files are missing" >&2
  exit 1
fi

if ! grep -q 'MIMI_CHAN_SYSTEM' "${BUS_H}"; then
  echo "FAIL: system channel constant is missing from message bus" >&2
  exit 1
fi

if ! grep -q 'strcmp(msg.channel, MIMI_CHAN_SYSTEM)' "${MIMI_C}"; then
  echo "FAIL: outbound dispatch does not handle system channel" >&2
  exit 1
fi

echo "PASS: system channel wiring is present."
