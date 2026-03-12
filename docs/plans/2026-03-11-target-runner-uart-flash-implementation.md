# Target Runner UART Flash Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extend the Target suite-addressable runner with Linux-safe `tkl_uart` and `tkl_flash` contract suites, plus matching pytest coverage and docs.

**Architecture:** Reuse the batch3 named-suite harness unchanged and add two more deterministic suite entries. Keep tests focused on contracts that are stable in the current Linux template implementations so local validation remains reliable without hardware setup.

**Tech Stack:** C, Unity, TuyaOpen `tos.py`, Python, pytest

---

### Task 1: Add the `tkl_uart` target suite

**Files:**
- Create: `test_app/unit_test_app/src/test_tkl_uart.c`
- Modify: `test_app/unit_test_app/src/test_registry.c`
- Modify: `test_app/unit_test_app/src/test_registry.h`

**Step 1: Write the failing suite test file**

Cover:

- `tkl_uart_write(2, ...) < 0`
- `tkl_uart_read(2, ...) < 0`
- `tkl_uart_set_tx_int(...) == OPRT_NOT_SUPPORTED`
- `tkl_uart_set_rx_flowctrl(...) == OPRT_NOT_SUPPORTED`
- `tkl_uart_wait_for_data(...) == OPRT_NOT_SUPPORTED`
- `tkl_uart_ioctl(...) == OPRT_NOT_SUPPORTED`

**Step 2: Register suite name `tkl_uart`**

Expose the suite through the named registry and declarations.

**Step 3: Run the suite to verify RED then GREEN**

Run:

```bash
cd test_app/unit_test_app
../../tos.py build
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_uart
```

Expected: suite exits cleanly and Unity reports `PASS`.

### Task 2: Add the `tkl_flash` target suite

**Files:**
- Create: `test_app/unit_test_app/src/test_tkl_flash.c`
- Modify: `test_app/unit_test_app/src/test_registry.c`
- Modify: `test_app/unit_test_app/src/test_registry.h`

**Step 1: Write the failing suite test file**

Cover:

- read/write before init -> `OPRT_RESOURCE_NOT_READY`
- `tkl_flash_get_one_type_info(..., NULL)` -> `OPRT_INVALID_PARM`
- invalid type -> `OPRT_INVALID_PARM`
- valid type info for `KV_DATA` and `UF`
- write/read round trip after init
- lock/unlock -> `OPRT_NOT_SUPPORTED`

**Step 2: Register suite name `tkl_flash`**

The suite must appear in `--list`.

**Step 3: Run the suite to verify RED then GREEN**

Run:

```bash
cd test_app/unit_test_app
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_flash
```

Expected: suite exits cleanly and Unity reports `PASS`.

### Task 3: Extend pytest coverage

**Files:**
- Create: `tests/target/pytest/test_tkl_uart.py`
- Create: `tests/target/pytest/test_tkl_flash.py`

**Step 1: Write the failing pytest cases**

Both tests should call `run_target_suite(...)` and assert suite begin/end markers.

**Step 2: Run pytest to verify failure**

Run:

```bash
python -m pytest tests/target/pytest -m target -v
```

Expected: new tests fail before the corresponding suites are fully wired.

**Step 3: Verify GREEN after suite wiring**

Run:

```bash
python -m pytest tests/target/pytest -m target -v
```

Expected: all Target pytest cases pass locally.

### Task 4: Update docs and progress notes

**Files:**
- Modify: `docs/testing/unit-test-guide.md`
- Modify: `tools/test/README.md`
- Modify: `tuyaopen_unit_test_scheme.md`

**Step 1: Record the added suites**

Document `tkl_uart` and `tkl_flash` as local named Target suites.

**Step 2: Update local validation commands**

Include direct ELF execution examples for:

- `--suite tkl_uart`
- `--suite tkl_flash`

**Step 3: Verify focused formatting and docs correctness**

Run:

```bash
python tools/check_format.py --debug --files \
  test_app/unit_test_app/src/test_tkl_uart.c \
  test_app/unit_test_app/src/test_tkl_flash.c
```

Expected: source formatting passes.
