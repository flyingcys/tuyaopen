import pytest


@pytest.mark.target
def test_tkl_uart_port(run_target_suite):
    output = run_target_suite("tkl_uart")

    assert "UT_SUITE_BEGIN:tkl_uart" in output
    assert "UT_SUITE_END:tkl_uart:OK" in output
    assert "FAIL" not in output
