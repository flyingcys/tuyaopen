import pytest


@pytest.mark.target
def test_tkl_gpio_port(run_target_suite):
    output = run_target_suite("tkl_gpio")

    assert "UT_SUITE_BEGIN:tkl_gpio" in output
    assert "UT_SUITE_END:tkl_gpio:OK" in output
    assert "FAIL" not in output
