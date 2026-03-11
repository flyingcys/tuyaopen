import pytest


@pytest.mark.target
@pytest.mark.smoke
def test_target_smoke(run_target_suite):
    output = run_target_suite("smoke")

    assert "UNITY_BEGIN" in output
    assert "UT_SUITE_BEGIN:smoke" in output
    assert "UT_TARGET_SMOKE_OK" in output
    assert "test_target_smoke_hello:PASS" in output
    assert "UT_SUITE_END:smoke:OK" in output
