import pytest


@pytest.mark.target
def test_tkl_flash_port(run_target_suite):
    output = run_target_suite("tkl_flash")

    assert "UT_SUITE_BEGIN:tkl_flash" in output
    assert "UT_SUITE_END:tkl_flash:OK" in output
    assert "FAIL" not in output
