import pytest


@pytest.mark.target
def test_tkl_i2c_port(run_target_suite):
    output = run_target_suite("tkl_i2c")

    assert "UT_SUITE_BEGIN:tkl_i2c" in output
    assert "UT_SUITE_END:tkl_i2c:OK" in output
    assert "FAIL" not in output
