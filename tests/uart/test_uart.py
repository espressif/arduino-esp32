def test_uart(dut):
    dut.expect_unity_test_output(timeout=120)
