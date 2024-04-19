def test_wifi(dut):
    dut.expect_unity_test_output(timeout=240)
