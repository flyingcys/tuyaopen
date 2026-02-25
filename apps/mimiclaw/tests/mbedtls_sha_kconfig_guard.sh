#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
KCONFIG_FILE="${ROOT_DIR}/src/libtls/Kconfig"
TLS_CFG_H="${ROOT_DIR}/src/libtls/port/tuya_tls_config.h"
MIMI_CFG_DIR="${ROOT_DIR}/apps/mimiclaw/config"

LINUX_CFG="${MIMI_CFG_DIR}/Linux.config"
T5AI_CFG="${MIMI_CFG_DIR}/T5AI.config"

for f in "${KCONFIG_FILE}" "${TLS_CFG_H}" "${LINUX_CFG}" "${T5AI_CFG}"; do
  if [[ ! -f "${f}" ]]; then
    echo "FAIL: missing file ${f}" >&2
    exit 1
  fi
done

if ! grep -q "config ENABLE_MBEDTLS_SHA512_C" "${KCONFIG_FILE}"; then
  echo "FAIL: Kconfig missing ENABLE_MBEDTLS_SHA512_C" >&2
  exit 1
fi

if ! grep -q "config ENABLE_MBEDTLS_SHA384_C" "${KCONFIG_FILE}"; then
  echo "FAIL: Kconfig missing ENABLE_MBEDTLS_SHA384_C" >&2
  exit 1
fi

if ! grep -A2 "config ENABLE_MBEDTLS_SHA512_C" "${KCONFIG_FILE}" | grep -q "default n"; then
  echo "FAIL: ENABLE_MBEDTLS_SHA512_C default is not n" >&2
  exit 1
fi

if ! grep -A3 "config ENABLE_MBEDTLS_SHA384_C" "${KCONFIG_FILE}" | grep -q "default n"; then
  echo "FAIL: ENABLE_MBEDTLS_SHA384_C default is not n" >&2
  exit 1
fi

if ! grep -A3 "config ENABLE_MBEDTLS_SHA384_C" "${KCONFIG_FILE}" | grep -q "select ENABLE_MBEDTLS_SHA512_C"; then
  echo "FAIL: ENABLE_MBEDTLS_SHA384_C must select ENABLE_MBEDTLS_SHA512_C" >&2
  exit 1
fi

if ! grep -q "#if defined(ENABLE_MBEDTLS_SHA512_C) || defined(ENABLE_MBEDTLS_SHA384_C)" "${TLS_CFG_H}"; then
  echo "FAIL: tuya_tls_config.h missing SHA512/SHA384 combined gate" >&2
  exit 1
fi

if ! grep -q "#ifdef ENABLE_MBEDTLS_SHA384_C" "${TLS_CFG_H}"; then
  echo "FAIL: tuya_tls_config.h missing ENABLE_MBEDTLS_SHA384_C gate" >&2
  exit 1
fi

if ! grep -q "#undef MBEDTLS_SHA512_C" "${TLS_CFG_H}"; then
  echo "FAIL: tuya_tls_config.h missing MBEDTLS_SHA512_C undef path" >&2
  exit 1
fi

if ! grep -q "#undef MBEDTLS_SHA384_C" "${TLS_CFG_H}"; then
  echo "FAIL: tuya_tls_config.h missing MBEDTLS_SHA384_C undef path" >&2
  exit 1
fi

for cfg in "${LINUX_CFG}" "${T5AI_CFG}"; do
  if ! grep -q "CONFIG_ENABLE_MBEDTLS_SHA512_C=y" "${cfg}"; then
    echo "FAIL: ${cfg} missing CONFIG_ENABLE_MBEDTLS_SHA512_C=y" >&2
    exit 1
  fi
  if ! grep -q "CONFIG_ENABLE_MBEDTLS_SHA384_C=y" "${cfg}"; then
    echo "FAIL: ${cfg} missing CONFIG_ENABLE_MBEDTLS_SHA384_C=y" >&2
    exit 1
  fi
done

echo "PASS: SHA384/SHA512 Kconfig and header wiring are in place."
