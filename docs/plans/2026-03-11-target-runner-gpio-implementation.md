# Target Runner GPIO Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Upgrade the Target test path from a smoke-only skeleton to a suite-addressable runner, and land the first real `tkl_gpio` Target suite.

**Architecture:** Introduce a target-suite registry with deterministic names, keep local `LINUX` execution argument-driven, and expose the same suite names to future serial-driven board execution. Expand `pytest` and flash runner code around this protocol rather than hardcoding one board.

**Tech Stack:** C, Unity, TuyaOpen `tos.py`, Python, pytest, subprocess, serial I/O

---

### Task 1: Normalize the target app harness

**Files:**
- Modify: `test_app/unit_test_app/src/test_main.c`
- Modify: `test_app/unit_test_app/src/test_registry.c`
- Modify: `test_app/unit_test_app/src/test_registry.h`
- Create: `test_app/unit_test_app/src/test_protocol.h`
- Create: `test_app/unit_test_app/src/test_protocol.c`

**Step 1: Add a suite descriptor structure**

Define one suite descriptor with:

- suite name
- help text
- run callback

**Step 2: Move the smoke test behind a named suite**

Make the existing smoke test callable as suite `smoke`.

**Step 3: Add argument-driven execution for Linux**

Support:

- `--list`
- `--suite <name>`
- `--all`

**Step 4: Preserve the current default**

No arguments should still run the smoke suite so existing smoke tests remain valid.

**Step 5: Verify locally**

Run:

```bash
cd test_app/unit_test_app
python ../../tos.py build
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --list
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite smoke
```

Expected:

- suite list prints `smoke`
- smoke suite passes

### Task 2: Add the first target-side `tkl_gpio` suite

**Files:**
- Create: `test_app/unit_test_app/src/test_tkl_gpio.c`
- Modify: `test_app/unit_test_app/src/test_registry.c`

**Step 1: Write contract-style tests first**

Cover:

- `tkl_gpio_init(..., NULL)` -> invalid parameter
- invalid pin id -> invalid parameter
- `tkl_gpio_read(..., NULL)` -> invalid parameter
- `tkl_gpio_irq_init(..., NULL)` -> invalid parameter
- invalid pin id for write/read/deinit/irq enable/irq disable

**Step 2: Register suite name `tkl_gpio`**

The suite must be visible in `--list`.

**Step 3: Keep it environment-safe**

Only add positive-path Linux GPIO cases if they can be gated behind clear runtime checks. The base suite must still pass on a machine without GPIO access.

**Step 4: Verify**

Run:

```bash
cd test_app/unit_test_app
./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_gpio
```

Expected:

- suite exits cleanly
- Unity output shows `PASS`

### Task 3: Make the pytest path suite-aware

**Files:**
- Modify: `tests/target/pytest/conftest.py`
- Modify: `tests/target/runners/serial_runner.py`
- Modify: `tests/target/pytest/test_smoke.py`
- Modify: `tests/target/pytest/test_tkl_gpio.py`

**Step 1: Add local process suite selection**

The local runner should support process args so pytest can request:

- `--list`
- `--suite smoke`
- `--suite tkl_gpio`

**Step 2: Add a helper fixture for named suite execution**

Tests should not duplicate process-launch logic.

**Step 3: Replace placeholder `test_tkl_gpio.py`**

Make it execute the named suite in local `LINUX` mode and assert successful Unity output.

**Step 4: Keep serial mode extensible**

If a serial target is used later, the same test should be able to send a named suite command instead of becoming a separate flow.

**Step 5: Verify**

Run:

```bash
python -m pytest tests/target/pytest -m target -v
```

Expected:

- smoke passes
- `tkl_gpio` no longer skips in local mode

### Task 4: Upgrade the flash runner contract

**Files:**
- Modify: `tests/target/runners/flash_runner.py`
- Modify: `docs/testing/unit-test-guide.md`

**Step 1: Support a command template**

Read a flash command template from environment, for example:

- `TUYA_TARGET_FLASH_CMD`

Template placeholders should support:

- `{image}`
- `{port}`
- `{project_dir}`

**Step 2: Execute only when explicitly requested**

If `--flash` is not used, no flashing occurs.

**Step 3: Fail clearly when flashing is requested but no command template is configured**

**Step 4: Verify**

Run:

```bash
python -m pytest tests/target/pytest -m target -v
```

Expected:

- local mode still passes without requiring flash

### Task 5: Update docs and plan status

**Files:**
- Modify: `docs/plans/2026-03-10-tuyaopen-unit-test-implementation.md`
- Modify: `tuyaopen_unit_test_scheme.md`
- Modify: `docs/testing/unit-test-guide.md`

**Step 1: Update current progress**

Record that:

- Target suites are now addressable by name
- `tkl_gpio` is the first real target suite
- flash runner now supports a command-template interface

**Step 2: Document the current target commands**

Include:

- local ELF invocation
- pytest invocation
- flash command environment variable

**Step 3: Verify docs format**

Run:

```bash
python tools/check_format.py --debug --files test_app/unit_test_app/src/test_main.c test_app/unit_test_app/src/test_registry.c test_app/unit_test_app/src/test_protocol.c test_app/unit_test_app/src/test_tkl_gpio.c
```

Expected:

- C/C++ formatting passes for new source files
