def test_cpu(dut):
    dut.expect_unity_test_output(timeout=120)
