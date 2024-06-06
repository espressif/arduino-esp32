def test_wifi(dut):
    dut.expect_exact("WiFi connected")
    dut.expect_exact("IP address:")
