def test_uart(dut):
    dut.expect('Hello from Serial1 (UART1) >>>  to >>> Serial (UART0)')
    dut.expect_unity_test_output(timeout=240)
