#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_HOME="$(mktemp -d)"
BACKUP_VENV=""
cleanup() {
    if [[ -n "$BACKUP_VENV" && -d "$BACKUP_VENV" ]]; then
        rm -rf "$ROOT_DIR/.venv"
        mv "$BACKUP_VENV" "$ROOT_DIR/.venv"
    fi
    rm -rf "$TMP_HOME"
}
trap cleanup EXIT

prepare_clean_venv_state() {
    if [[ -d "$ROOT_DIR/.venv" ]]; then
        BACKUP_VENV="$(mktemp -d "${TMP_HOME}/venv-backup.XXXXXX")"
        rm -rf "$BACKUP_VENV"
        mv "$ROOT_DIR/.venv" "$BACKUP_VENV"
    fi
    rm -rf "$ROOT_DIR/.venv"
}

assert_contains() {
    local haystack="$1"
    local needle="$2"
    if [[ "$haystack" != *"$needle"* ]]; then
        printf 'assertion failed: expected output to contain: %s\n' "$needle" >&2
        exit 1
    fi
}

assert_not_contains() {
    local haystack="$1"
    local needle="$2"
    if [[ "$haystack" == *"$needle"* ]]; then
        printf 'assertion failed: expected output to not contain: %s\n' "$needle" >&2
        exit 1
    fi
}

test_zsh_uses_supported_export_flow() {
    local output
    prepare_clean_venv_state
    output="$(
        cd "$ROOT_DIR"
        HOME="$TMP_HOME" zsh -lc '
            unset VIRTUAL_ENV OPEN_SDK_ROOT OPEN_SDK_PYTHON OPEN_SDK_PIP
            . ./export.sh
        ' 2>&1
    )"

    assert_not_contains "$output" "invalid option"
    assert_contains "$output" "Virtual environment activated successfully"
}

test_prefers_python_3_10_when_available() {
    local output
    rm -rf "$ROOT_DIR/.venv"
    output="$(
        cd "$ROOT_DIR"
        HOME="$TMP_HOME" bash -lc '
            unset VIRTUAL_ENV OPEN_SDK_ROOT OPEN_SDK_PYTHON OPEN_SDK_PIP
            . ./export.sh
        ' 2>&1
    )"

    assert_contains "$output" "Using python3.10 (Python 3.10"
    assert_not_contains "$output" "No matching distribution found for PyYAML==6.0.2"
    assert_not_contains "$output" "ModuleNotFoundError"
}

test_zsh_uses_supported_export_flow
test_prefers_python_3_10_when_available

printf 'test_export_sh.sh: PASS\n'
