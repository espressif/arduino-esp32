def test_nvs(dut):
     dut.expect('Current counter value: 0')
     dut.expect('Current counter value: 1')
     dut.expect('Current counter value: 2')
     dut.expect('Current counter value: 3')
     dut.expect('Current counter value: 4')
     dut.expect('Current counter value: 5')