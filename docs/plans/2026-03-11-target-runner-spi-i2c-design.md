# Target Runner SPI I2C Design

**Context**

The Target harness already supports named local suites for:

- `smoke`
- `tkl_gpio`
- `tkl_uart`
- `tkl_flash`

The next useful expansion is to validate two more adapter families that are widely depended on by higher-level drivers:

- `tkl_spi`
- `tkl_i2c`

## Recommended Approach

Add one Linux-safe contract suite for each adapter and keep the scope limited to behavior that is deterministic without `/dev/spidev*` or `/dev/i2c-*` devices.

This batch should validate:

- parameter rejection
- `NOT_SUPPORTED` surfaces that are intentionally stubbed on Linux
- safe status and metadata APIs that do not require bus access

## Alternatives Considered

### 1. Add real transfer tests using local device nodes

Rejected because this makes test success dependent on host hardware and device permissions, which breaks the current local `LINUX` regression contract.

### 2. Add only SPI or only I2C

Lower risk, but it does not prove the suite registry pattern continues scaling across another pair of adapter domains.

### 3. Add Linux-safe `tkl_spi` and `tkl_i2c` contract suites now

Recommended.

This keeps the batch deterministic while extending coverage to two more driver-facing interfaces.

## Design

### `tkl_spi`

Use only stable contract checks:

- `tkl_spi_init(..., NULL)` -> `OPRT_INVALID_PARM`
- invalid port id for init/deinit/send/recv/transfer/transfer_with_length -> `OPRT_INVALID_PARM`
- non-master role in init -> `OPRT_NOT_SUPPORTED`
- `tkl_spi_get_status(..., NULL)` -> `OPRT_INVALID_PARM`
- `tkl_spi_get_status(..., &status)` -> `OPRT_OK` and zeroed status
- `tkl_spi_irq_*` -> `OPRT_NOT_SUPPORTED`
- `tkl_spi_ioctl(...)` -> `OPRT_NOT_SUPPORTED`
- `tkl_spi_abort_transfer(...)` -> `OPRT_OK`
- `tkl_spi_get_data_count(invalid)` -> `< 0`
- `tkl_spi_get_max_dma_data_length()` -> `0`

Avoid open-device positive paths in this batch because Linux SPI initialization depends on `/dev/spidev*`.

### `tkl_i2c`

Use similarly stable checks:

- `tkl_i2c_init(..., NULL)` -> `OPRT_INVALID_PARM`
- invalid port id for init/deinit/master_send/master_receive -> `OPRT_INVALID_PARM`
- `tkl_i2c_master_receive(..., NULL, ...)` -> `OPRT_INVALID_PARM`
- `tkl_i2c_master_receive(..., size=0)` -> `OPRT_INVALID_PARM`
- `tkl_i2c_irq_*` -> `OPRT_NOT_SUPPORTED`
- `tkl_i2c_set_slave_addr/slave_send/slave_receive` -> `OPRT_NOT_SUPPORTED`
- `tkl_i2c_get_status(..., NULL)` -> `OPRT_INVALID_PARM`
- `tkl_i2c_get_status(..., &status)` -> `OPRT_NOT_SUPPORTED`
- `tkl_i2c_reset(...)` -> `OPRT_OK`
- `tkl_i2c_get_data_count(invalid)` -> `< 0`
- `tkl_i2c_ioctl(...)` -> `OPRT_NOT_SUPPORTED`

Avoid device-node-dependent positive-path master transfers in this batch.

### Pytest

Add:

- `tests/target/pytest/test_tkl_spi.py`
- `tests/target/pytest/test_tkl_i2c.py`

Reuse the existing `run_target_suite(...)` fixture.

### Docs

Update the Target docs to list the two new suite names and extend the direct ELF examples.

## Verification

This batch is complete only if all of the following are true:

1. `cd test_app/unit_test_app && ../../tos.py build`
2. `./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_spi`
3. `./dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_i2c`
4. `python -m pytest tests/target/pytest -m target -v`

## Expected Outcome

After this batch:

- the Target registry includes six named suites
- local `LINUX` pytest covers `smoke`, `tkl_gpio`, `tkl_uart`, `tkl_flash`, `tkl_spi`, and `tkl_i2c`
- the next remaining gap is real-board serial execution and positive-path bus validation
