def test_wifi(dut):
    dut.expect_exact("Scan start")
    dut.expect_exact("Scan done")
    dut.expect_exact("Wokwi-GUEST")
    dut.expect_exact("WiFi connected")
    dut.expect_exact("IP address:")
