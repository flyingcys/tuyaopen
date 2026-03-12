# TuyaOpen Unit Test Guide

## Overview

TuyaOpen currently has two unit-test paths:

- `Host` unit tests for fast logic verification with `Unity + CMock + CTest`
- `Target` unit-test harness for `tos.py build` and `pytest`-driven validation

At the moment, the stable regression baseline is still the `Host` path. The `Target` path now supports named suites on `LINUX` first and keeps the same suite names for future real-board serial execution.

## Repository Layout

```text
tuyaopen/
├── tests/
│   ├── host/
│   │   ├── CMakeLists.txt
│   │   ├── cmake/
│   │   ├── include/
│   │   └── support/
│   ├── target/
│   │   ├── pytest/
│   │   └── runners/
│   └── vendor/
│       ├── cmock/
│       └── unity/
├── test_app/
│   └── unit_test_app/
├── tools/
│   └── test/
└── src/
    └── <component>/test/host/
```

Key points:

- Component Host tests live near the source under `src/<component>/test/host/`
- Shared Host test build helpers live under `tests/host/`
- Target-side pytest entrypoints live under `tests/target/pytest/`
- The current target test application lives under `test_app/unit_test_app/`

## Environment Setup

Run from the repository root:

```bash
. ./export.sh
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
```

Notes:

- `export.sh` creates or reuses `.venv/`
- It installs dependencies from `requirements.txt`
- It exports `OPEN_SDK_ROOT`, `OPEN_SDK_PYTHON`, and `OPEN_SDK_PIP`
- The `.dont_prompt_update_platform` file prevents interactive update prompts during `tos.py` commands

If you are working in a fresh worktree, initialize the relevant submodules before running tests:

```bash
git submodule update --init tests/vendor/unity tests/vendor/cmock src/tal_kv/littlefs
```

## Host Unit Tests

### Quick Run

```bash
bash tools/test/run_host_tests.sh
```

This command:

1. Configures `tests/host`
2. Builds generated mocks through `tuya_codegen`
3. Builds the registered host test executables
4. Runs them through `ctest`

### Coverage

```bash
bash tools/test/run_host_coverage.sh
```

Coverage output is generated under:

- `build/tests/host/coverage_html/`

### Current Host Suites

The current baseline includes:

- `src/common/test/host`
- `src/tal_system/test/host`
- `src/tal_kv/test/host`
- `src/tal_network/test/host`
- `src/tal_wifi/test/host`
- `src/tuya_cloud_service/test/host`
  Current coverage: `tuya_endpoint`, `tuya_transport`, `tuya_protocol`, `tuya_health`, `tuya_iot_dp`, `tuya_weather`, `tuya_http`, `tuya_tls`, `tuya_iot`, `mqtt_service`, `atop_service`, `matop_service`, `tuya_lan`, `tuya_authorize`, `netcfg`, `netmgr`
  Phase 5 deepened coverage:
  - `tuya_lan`: white-box Host suite, currently `100%` line coverage / `100%` function coverage for `tuya_lan.c`; covers callback register/unregister, session lifecycle, TCP/UDP callbacks, handshake, DP report, `init/exit`, activated `enable/disable`, no-session report/query paths
  - `tuya_lan`: a small number of recovery-only / Host-hard-to-drive branches are explicitly marked with `LCOV_EXCL_*` in source so coverage reports stay stable and intentional
  - `tuya_authorize`: KV write success/failure, KV-first read path, OTP fallback, reset error propagation, CLI `auth` / `auth-read` / `auth-reset`
  - `netcfg`: registration counts, duplicate reject, started-state transitions, `start_other_all` / `stop_other_all`, stop-all and failure-tolerant error paths
  - `netmgr`: internal connection registration, no-connection init failure, active-connection dispatch, timer callback, missing callback and get-error validation

Current Host baseline:

- `21/21` suites passing

## Target Unit Test Skeleton

### Build the Target Test App

```bash
cd test_app/unit_test_app
tos.py check
tos.py build
```

Artifacts:

- Build intermediates: `test_app/unit_test_app/.build/`
- Final output: `test_app/unit_test_app/dist/`

The current default configuration targets `LINUX/Ubuntu`, so the first validation path is a locally runnable executable.

### Run the Pytest Target Suites

From the repository root:

```bash
python -m pytest tests/target/pytest -m target -v
```

Behavior:

- Without `--target-port`, pytest discovers and launches the local `LINUX` executable from `dist/` or `.build/bin/`
- With `--target-port`, pytest opens the specified serial device and treats it as the DUT
- With `--flash`, pytest invokes the flash runner before opening the port

Flash command injection:

```bash
export TUYA_TARGET_FLASH_CMD='python tools/flash_board.py --image {image} --port {port}'
```

Supported placeholders:

- `{image}`
- `{port}`
- `{project_dir}`

Useful options:

```bash
python -m pytest tests/target/pytest -m target -v \
  --target-app-path test_app/unit_test_app \
  --target-image test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app
```

```bash
python -m pytest tests/target/pytest -m target -v \
  --target-port /dev/ttyUSB0 \
  --target-baudrate 115200
```

The local target executable also supports direct suite control:

```bash
cd test_app/unit_test_app
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --list
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite smoke
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_gpio
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_uart
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_flash
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_spi
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_i2c
```

Current named local Target suites:

- `smoke`
- `tkl_gpio`
- `tkl_uart`
- `tkl_flash`
- `tkl_spi`
- `tkl_i2c`

Current local `LINUX` Target pytest baseline:

- `6 passed`

## Common Workflows

### 1. Modify Existing Host Tests

```bash
bash tools/test/run_host_tests.sh
```

Use this when you only touched existing Host test code or component logic already covered by Host suites.

### 2. Add a New Host Test Suite

1. Create `src/<component>/test/host/`
2. Add the component `CMakeLists.txt`
3. Add one or more `test_*.c`
4. Register the directory in `tests/host/CMakeLists.txt`
5. Run:

```bash
bash tools/test/run_host_tests.sh
```

### 3. Modify Target Test Infrastructure

When you change `test_app/unit_test_app`, `tests/target/pytest`, or `tests/target/runners`, run both:

```bash
cd test_app/unit_test_app
tos.py check
tos.py build
```

```bash
cd ../..
python -m pytest tests/target/pytest -m target -v
```

If you touched a named target suite, also run it directly once:

```bash
cd test_app/unit_test_app
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_gpio
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_uart
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_flash
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_spi
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_i2c
```

### 4. Documentation-Only Updates

For testing docs, run a focused format check:

```bash
python tools/check_format.py --debug --files \
  README.md README_zh.md tuyaopen_unit_test_scheme.md \
  tools/test/README.md .github/pull_request_template.md \
  docs/testing/unit-test-guide.md docs/testing/component-test-template.md
```

## Guidelines

- Keep component tests close to the component, not in a giant root-level test bucket
- Prefer `Host` tests first for TAL/common logic
- Treat `Target` tests as hardware-reality validation, not the main fast feedback loop
- Reuse `tests/host/cmake/AddTuyaHostTest.cmake` and `tests/host/cmake/GenerateMocks.cmake`
- When changing logic or behavior, update tests in the same change whenever practical
