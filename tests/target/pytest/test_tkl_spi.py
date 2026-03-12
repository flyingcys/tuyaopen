import pytest


@pytest.mark.target
def test_tkl_spi_port(run_target_suite):
    output = run_target_suite("tkl_spi")

    assert "UT_SUITE_BEGIN:tkl_spi" in output
    assert "UT_SUITE_END:tkl_spi:OK" in output
    assert "FAIL" not in output
