# Target Runner UART Flash Design

**Context**

The Target test path already supports:

- named suite discovery
- local `LINUX` execution through `--suite`
- pytest-driven suite selection
- one stable adapter contract suite: `tkl_gpio`

The next missing layer is to prove the runner pattern scales beyond one suite and to cover two more adapter boundaries that matter to later board validation:

- `tkl_uart`
- `tkl_flash`

## Recommended Approach

Add two more named Target suites and keep them Linux-safe by focusing on stable adapter contracts instead of board-dependent positive paths.

The suites should verify behavior that is deterministic in the current `tools/porting/template/linux/` implementations:

- `tkl_uart`: unsupported feature returns and invalid runtime paths that do not require opening a real UART
- `tkl_flash`: partition-info queries, pre-init behavior, file-backed read/write behavior, and unsupported lock APIs

`pytest` should continue using the named-suite fixture pattern introduced in batch3, with one test file per suite.

## Alternatives Considered

### 1. Add full UART loopback and flash persistence integration tests now

Rejected because the Linux UART template uses stdin and hard-coded UDP endpoints, which makes positive-path tests fragile in CI and unsafe without environment preparation.

### 2. Add only one new suite first

Lowest short-term risk, but it does not prove the registry and pytest pattern scales to multiple new suites in one batch.

### 3. Add `tkl_uart` and `tkl_flash` contract suites now

Recommended.

This keeps the batch small, deterministic, and aligned with the current Target harness role: adapter-level validation on local `LINUX`, with room to deepen board-specific cases later.

## Design

### Target app

Register two new suites:

- `tkl_uart`
- `tkl_flash`

Both suites should follow the same pattern as `tkl_gpio`: one suite entry function, Unity-managed test cases, and deterministic suite names exposed through `--list`.

### `tkl_uart` scope

Keep the suite limited to Linux-safe contracts that do not require actual UART devices:

- `tkl_uart_write` on an unsupported port returns `< 0`
- `tkl_uart_read` on an unsupported port returns `< 0`
- `tkl_uart_set_tx_int` returns `OPRT_NOT_SUPPORTED`
- `tkl_uart_set_rx_flowctrl` returns `OPRT_NOT_SUPPORTED`
- `tkl_uart_wait_for_data` returns `OPRT_NOT_SUPPORTED`
- `tkl_uart_ioctl` returns `OPRT_NOT_SUPPORTED`

Avoid calling `tkl_uart_init` on the current Linux template in this batch because port `0` touches stdin state and port `1` binds to a hard-coded address.

### `tkl_flash` scope

Use the file-backed Linux flash template to validate stable contract behavior:

- read/write before initialization returns `OPRT_RESOURCE_NOT_READY`
- `tkl_flash_get_one_type_info(..., NULL)` returns `OPRT_INVALID_PARM`
- invalid flash type returns `OPRT_INVALID_PARM`
- valid type info contains expected partition metadata
- after type-info init, write then read round-trips bytes
- lock/unlock return `OPRT_NOT_SUPPORTED`

Avoid asserting erase semantics in this batch because the Linux template currently returns success without clearing bytes.

### Pytest

Add:

- `tests/target/pytest/test_tkl_uart.py`
- `tests/target/pytest/test_tkl_flash.py`

Both should use `run_target_suite(...)` and assert:

- `UT_SUITE_BEGIN:<name>`
- `UT_SUITE_END:<name>:OK`
- no `FAIL` markers

### Documentation

Update docs to record:

- `tkl_uart` and `tkl_flash` as additional Target suites
- the expanded local validation commands
- the next remaining gap: real-board serial execution for these suites

## Verification

This batch is complete only if all of the following are true:

1. `cd test_app/unit_test_app && ../../tos.py build`
2. `./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --list`
3. `./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_uart`
4. `./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_flash`
5. `python -m pytest tests/target/pytest -m target -v`

## Expected Outcome

After this batch:

- the Target suite registry covers more than one real adapter family
- pytest validates four named local suites: `smoke`, `tkl_gpio`, `tkl_uart`, `tkl_flash`
- the repository has a repeatable pattern for future `tkl_spi`, `tkl_i2c`, and board-specific expansion
