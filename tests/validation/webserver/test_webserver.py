def test_webserver(dut):
    dut.expect_unity_test_output(timeout=120)
