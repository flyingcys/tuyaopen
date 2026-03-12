# Host Unit Test Scripts

This directory contains helper scripts for host-side unit testing.

Initialize the environment from the repository root before running any command:

```bash
. ./export.sh
```

## Run Host Unit Tests

```bash
bash tools/test/run_host_tests.sh
```

## Run Host Coverage

```bash
bash tools/test/run_host_coverage.sh
```

The coverage script will generate HTML output under:

- `build/tests/host/coverage_html/`

## Build the Target Unit Test App

```bash
mkdir -p .cache && touch .cache/.dont_prompt_update_platform
cd test_app/unit_test_app
tos.py check
tos.py build
```

Build intermediates are generated under:

- `test_app/unit_test_app/.build/`

Final artifacts are exported under:

- `test_app/unit_test_app/dist/`

## Run Target Pytest Suites

Run against the locally built `LINUX` executable:

```bash
python -m pytest tests/target/pytest -m target -v
```

Current local `LINUX` Target pytest baseline:

- `6 passed`

Run named suites directly from the built ELF:

```bash
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --list
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite smoke
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_gpio
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_uart
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_flash
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_spi
./test_app/unit_test_app/dist/unit_test_app_1.0.0/unit_test_app_1.0.0.elf --suite tkl_i2c
```

Run against a real board over serial:

```bash
python -m pytest tests/target/pytest -m target -v --target-port /dev/ttyUSB0
```

Additional options:

- `--target-app-path <path>`: override the default `test_app/unit_test_app` path
- `--target-image <path>`: run a specific built image or executable
- `--target-baudrate <baud>`: set a custom serial baudrate
- `--flash`: execute the configured flash command before opening the serial port

Configure flashing through an environment variable:

```bash
export TUYA_TARGET_FLASH_CMD='python tools/flash_board.py --image {image} --port {port}'
```

Supported placeholders:

- `{image}`
- `{port}`
- `{project_dir}`
