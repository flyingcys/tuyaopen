import pytest


@pytest.mark.target
@pytest.mark.smoke
def test_target_smoke(dut):
    dut.expect("UNITY_BEGIN")
    dut.expect("UT_TARGET_SMOKE_OK")
    dut.expect("test_target_smoke_hello:PASS")
    dut.expect("OK")
