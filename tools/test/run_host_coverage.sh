#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
BUILD_DIR="${ROOT_DIR}/build/tests/host"
REPORT_DIR="${BUILD_DIR}/coverage_html"

cmake -S "${ROOT_DIR}/tests/host" -B "${BUILD_DIR}" -DTUYA_ENABLE_COVERAGE=ON
cmake --build "${BUILD_DIR}" --target tuya_codegen -j
cmake --build "${BUILD_DIR}" -j
cmake --build "${BUILD_DIR}" --target coverage

mkdir -p "${REPORT_DIR}"

if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
    lcov --capture --directory "${BUILD_DIR}" --output-file "${BUILD_DIR}/coverage.info"
    lcov \
        --remove "${BUILD_DIR}/coverage.info" \
        '/usr/*' \
        '*/tests/vendor/*' \
        '*/tests/host/*' \
        --output-file "${BUILD_DIR}/coverage_filtered.info"
    genhtml "${BUILD_DIR}/coverage_filtered.info" --output-directory "${REPORT_DIR}"
elif command -v gcovr >/dev/null 2>&1; then
    gcovr \
        -r "${ROOT_DIR}" \
        --html \
        --html-details \
        -o "${REPORT_DIR}/index.html" \
        --exclude "${ROOT_DIR}/tests/vendor" \
        "${BUILD_DIR}"
else
    echo "Neither lcov/genhtml nor gcovr is available. Skip HTML report generation."
fi
