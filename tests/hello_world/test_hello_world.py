def test_hello_arduino(dut):
    dut.expect('Hello Arduino!')
