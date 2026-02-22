#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
AGENT_C="${APP_DIR}/agent/agent_loop.c"

if [[ ! -f "${AGENT_C}" ]]; then
  echo "FAIL: ${AGENT_C} not found" >&2
  exit 1
fi

if grep -q "working_phrases\\[tal_system_get_random(phrase_count)\\]" "${AGENT_C}"; then
  echo "FAIL: agent_loop.c uses inclusive tal_system_get_random(phrase_count) as array index" >&2
  exit 1
fi

if ! grep -q "bounded_random_index" "${AGENT_C}"; then
  echo "FAIL: agent_loop.c does not provide bounded random index helper" >&2
  exit 1
fi

echo "PASS: agent random phrase index is safely bounded."
