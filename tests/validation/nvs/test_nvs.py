def test_nvs(dut):
    dut.expect_exact("Current counter value: 0")
    dut.expect_exact("Current counter value: 1")
    dut.expect_exact("Current counter value: 2")
