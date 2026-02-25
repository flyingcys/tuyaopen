#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"

if [[ ! -f "${CLI_C}" ]]; then
  echo "FAIL: ${CLI_C} not found" >&2
  exit 1
fi

if grep -q "char buf\\[4096\\]" "${CLI_C}"; then
  echo "FAIL: serial_cli.c still allocates 4KB memory_read buffer on CLI stack" >&2
  exit 1
fi

if grep -q "char content\\[2048\\]" "${CLI_C}"; then
  echo "FAIL: serial_cli.c still allocates 2KB memory_write buffer on CLI stack" >&2
  exit 1
fi

if ! grep -q "tal_malloc(4096)" "${CLI_C}"; then
  echo "FAIL: serial_cli.c does not allocate memory_read buffer from heap" >&2
  exit 1
fi

if ! grep -q "tal_malloc(2048)" "${CLI_C}"; then
  echo "FAIL: serial_cli.c does not allocate memory_write buffer from heap" >&2
  exit 1
fi

echo "PASS: CLI memory read/write buffers are heap-based."
