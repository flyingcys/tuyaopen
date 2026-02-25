#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CLI_C="${APP_DIR}/cli/serial_cli.c"

if [[ ! -f "${CLI_C}" ]]; then
  echo "FAIL: ${CLI_C} not found" >&2
  exit 1
fi

if ! grep -Fq '.name = "memory_clear"' "${CLI_C}"; then
  echo "FAIL: memory_clear is not registered in CLI" >&2
  exit 1
fi

if ! grep -Fq 'usage: memory_clear' "${CLI_C}"; then
  echo "FAIL: memory_clear usage is missing" >&2
  exit 1
fi

if ! grep -Fq 'memory_write_long_term("")' "${CLI_C}"; then
  echo "FAIL: memory_clear does not clear MEMORY.md via memory_write_long_term(\"\")" >&2
  exit 1
fi

echo "PASS: memory_clear command is wired and clears MEMORY.md."
