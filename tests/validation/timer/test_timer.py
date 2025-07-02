def test_timer(dut):
    dut.expect_unity_test_output(timeout=240)
