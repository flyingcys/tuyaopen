#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BIN_PATH="${ROOT_DIR}/platform/LINUX/build/mimiclaw"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "SKIP: this test is Linux-only."
  exit 0
fi

if ! command -v strace >/dev/null 2>&1; then
  echo "SKIP: strace not found."
  exit 0
fi

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "ERROR: binary not found: ${BIN_PATH}" >&2
  echo "Hint: run build first: . ${ROOT_DIR}/export.sh && cd ${ROOT_DIR}/apps/mimiclaw && tos.py build" >&2
  exit 2
fi

RAW_LOG="$(mktemp)"
TRACE_PREFIX="$(mktemp -u)"
COMBINED_TRACE="$(mktemp)"
trap 'rm -f "${RAW_LOG}" "${COMBINED_TRACE}" "${TRACE_PREFIX}"*' EXIT

set +e
timeout 12s strace -ff -tt -s 128 -e trace=network -o "${TRACE_PREFIX}" "${BIN_PATH}" >"${RAW_LOG}" 2>&1
run_rc=$?
set -e

if [[ ${run_rc} -ne 0 && ${run_rc} -ne 124 ]]; then
  echo "ERROR: mimiclaw exited unexpectedly: rc=${run_rc}" >&2
  cat "${RAW_LOG}" >&2
  exit 1
fi

cat "${TRACE_PREFIX}"* > "${COMBINED_TRACE}"

if ! grep -q "TUYA_TLS Begin Connect api.telegram.org:443" "${RAW_LOG}"; then
  echo "FAIL: telegram TLS stage was not reached; test is inconclusive." >&2
  cat "${RAW_LOG}" >&2
  exit 1
fi

if grep -q "SO_BINDTODEVICE" "${COMBINED_TRACE}"; then
  echo "FAIL: found SO_BINDTODEVICE in runtime network path; this bypasses system routing/tun." >&2
  grep "SO_BINDTODEVICE" "${COMBINED_TRACE}" >&2
  exit 1
fi

echo "PASS: no SO_BINDTODEVICE on outbound network path."
