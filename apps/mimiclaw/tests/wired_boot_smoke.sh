#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BIN_PATH="${ROOT_DIR}/platform/LINUX/build/mimiclaw"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "ERROR: binary not found: ${BIN_PATH}" >&2
  echo "Hint: run build first: . ${ROOT_DIR}/export.sh && cd ${ROOT_DIR}/apps/mimiclaw && tos.py build" >&2
  exit 2
fi

RAW_LOG="$(mktemp)"
CLEAN_LOG="$(mktemp)"
trap 'rm -f "${RAW_LOG}" "${CLEAN_LOG}"' EXIT

set +e
timeout 10s "${BIN_PATH}" >"${RAW_LOG}" 2>&1
run_rc=$?
set -e

# timeout(124) is expected because mimiclaw runs as a daemon-style loop.
if [[ ${run_rc} -ne 0 && ${run_rc} -ne 124 ]]; then
  echo "ERROR: mimiclaw exited unexpectedly: rc=${run_rc}" >&2
  cat "${RAW_LOG}" >&2
  exit 1
fi

# Strip ANSI color sequences for stable matching.
sed -E 's/\x1B\[[0-9;]*[[:alpha:]]//g' "${RAW_LOG}" >"${CLEAN_LOG}"

if ! grep -q "online services started in wired mode" "${CLEAN_LOG}"; then
  echo "FAIL: wired mode did not start online services." >&2
  echo "Expected log: online services started in wired mode" >&2
  echo "--- runtime log ---" >&2
  cat "${CLEAN_LOG}" >&2
  exit 1
fi

echo "PASS: wired mode online services started."
