from __future__ import annotations

import sys
from pathlib import Path

import pytest

RUNNERS_DIR = Path(__file__).resolve().parents[1] / "runners"
if str(RUNNERS_DIR) not in sys.path:
    sys.path.insert(0, str(RUNNERS_DIR))

from flash_runner import FlashRunner, discover_default_binary
from serial_runner import SerialRunner


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption("--target-app-path", action="store", default=None, help="Path to the target unit test app")
    parser.addoption("--target-port", action="store", default=None, help="Serial port for a real target")
    parser.addoption("--target-baudrate", action="store", type=int, default=115200, help="Serial baudrate")
    parser.addoption("--target-image", action="store", default=None, help="Explicit built image/executable path")
    parser.addoption("--flash", action="store_true", help="Flash image before opening the target port")


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


@pytest.fixture(scope="session")
def target_app_path(request: pytest.FixtureRequest, repo_root: Path) -> Path:
    value = request.config.getoption("--target-app-path")
    if value:
        return Path(value).resolve()

    return repo_root / "test_app" / "unit_test_app"


@pytest.fixture(scope="session")
def target_image(request: pytest.FixtureRequest, target_app_path: Path) -> Path | None:
    explicit_image = request.config.getoption("--target-image")
    if explicit_image:
        return Path(explicit_image).resolve()

    return discover_default_binary(target_app_path)


@pytest.fixture()
def dut(request: pytest.FixtureRequest, target_app_path: Path, target_image: Path | None):
    target_port = request.config.getoption("--target-port")
    flash_first = request.config.getoption("--flash")
    baudrate = request.config.getoption("--target-baudrate")

    if target_port:
        if flash_first:
            flasher = FlashRunner(target_app_path, target_image)
            try:
                flasher.flash(target_port)
            except RuntimeError as exc:
                pytest.skip(str(exc))

        runner = SerialRunner.from_port(target_port, baudrate=baudrate)
    else:
        if target_image is None:
            pytest.skip("No target port specified and no local executable found under dist/ or .build/bin/")

        runner = SerialRunner.from_process([str(target_image)], cwd=target_app_path)

    try:
        yield runner
    finally:
        runner.close()


@pytest.fixture()
def run_target_suite(
    request: pytest.FixtureRequest,
    target_app_path: Path,
    target_image: Path | None,
):
    target_port = request.config.getoption("--target-port")
    flash_first = request.config.getoption("--flash")
    baudrate = request.config.getoption("--target-baudrate")

    def _run(suite_name: str) -> str:
        if target_port:
            if flash_first:
                flasher = FlashRunner(target_app_path, target_image)
                flasher.flash(target_port)

            runner = SerialRunner.from_port(target_port, baudrate=baudrate)
            try:
                runner.write(f"ut_run {suite_name}\r\n")
                runner.expect(f"UT_SUITE_BEGIN:{suite_name}")
                runner.expect(f"UT_SUITE_END:{suite_name}:")
                return "\n".join(runner.history)
            finally:
                runner.close()

        if target_image is None:
            pytest.skip("No target image found for local suite execution")

        runner = SerialRunner.from_process(
            [str(target_image), "--suite", suite_name],
            cwd=target_app_path,
        )
        try:
            runner.expect(f"UT_SUITE_BEGIN:{suite_name}")
            runner.expect(f"UT_SUITE_END:{suite_name}:")
            return "\n".join(runner.history)
        finally:
            runner.close()

    return _run
