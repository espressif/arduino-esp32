def test_nvs(dut):
    dut.expect("Current counter value: 0")
    dut.expect("Current counter value: 1")
    dut.expect("Current counter value: 2")
