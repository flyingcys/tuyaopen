#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
BUILD_DIR="${ROOT_DIR}/build/tests/host"

cmake -S "${ROOT_DIR}/tests/host" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --target tuya_codegen -j
cmake --build "${BUILD_DIR}" -j
ctest --test-dir "${BUILD_DIR}" --output-on-failure
