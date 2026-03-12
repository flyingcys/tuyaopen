# Target Runner SPI I2C Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extend the Target named-suite runner with Linux-safe `tkl_spi` and `tkl_i2c` contract suites plus matching pytest coverage and docs.

**Architecture:** Reuse the existing named-suite harness and add two more deterministic suites that assert only stable Linux adapter contracts. Avoid device-node-dependent transfer success paths so local Target regression remains reliable.

**Tech Stack:** C, Unity, TuyaOpen `tos.py`, Python, pytest

---

### Task 1: Add the `tkl_spi` target suite

**Files:**
- Create: `test_app/unit_test_app/src/test_tkl_spi.c`
- Modify: `test_app/unit_test_app/src/test_registry.c`
- Modify: `test_app/unit_test_app/src/test_registry.h`

**Step 1: Write the failing suite test**

Cover:

- invalid/null parameter paths
- non-master role rejected by init
- `get_status`, `irq`, `ioctl`, `abort_transfer`, `get_data_count`, `get_max_dma_data_length`

**Step 2: Register suite name `tkl_spi`**

Expose the suite in the registry and declarations.

**Step 3: Run and verify**

Run:

```bash
cd test_app/unit_test_app
../../tos.py build
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_spi
```

Expected: Unity reports `PASS`.

### Task 2: Add the `tkl_i2c` target suite

**Files:**
- Create: `test_app/unit_test_app/src/test_tkl_i2c.c`
- Modify: `test_app/unit_test_app/src/test_registry.c`
- Modify: `test_app/unit_test_app/src/test_registry.h`

**Step 1: Write the failing suite test**

Cover:

- invalid/null parameter paths
- `master_receive` invalid buffer/size
- `irq`, slave APIs, `get_status`, `reset`, `get_data_count`, `ioctl`

**Step 2: Register suite name `tkl_i2c`**

The suite must appear in `--list`.

**Step 3: Run and verify**

Run:

```bash
cd test_app/unit_test_app
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_i2c
```

Expected: Unity reports `PASS`.

### Task 3: Extend pytest coverage

**Files:**
- Create: `tests/target/pytest/test_tkl_spi.py`
- Create: `tests/target/pytest/test_tkl_i2c.py`

**Step 1: Write the failing pytest tests**

Both should call `run_target_suite(...)` and assert begin/end markers.

**Step 2: Run pytest to verify RED**

Run:

```bash
python -m pytest tests/target/pytest -m target -v
```

Expected: the new tests fail before suite registration is complete.

**Step 3: Run pytest to verify GREEN**

Run:

```bash
python -m pytest tests/target/pytest -m target -v
```

Expected: all Target pytest cases pass.

### Task 4: Update docs and progress notes

**Files:**
- Modify: `docs/testing/unit-test-guide.md`
- Modify: `tools/test/README.md`
- Modify: `tuyaopen_unit_test_scheme.md`

**Step 1: Document the new suite names**

Add `tkl_spi` and `tkl_i2c` to the list of local Target suites.

**Step 2: Extend direct ELF examples**

Add `--suite tkl_spi` and `--suite tkl_i2c`.

**Step 3: Verify formatting**

Run:

```bash
python tools/check_format.py --debug --files \
  test_app/unit_test_app/src/test_tkl_spi.c \
  test_app/unit_test_app/src/test_tkl_i2c.c
```

Expected: formatting passes.
