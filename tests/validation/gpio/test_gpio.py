def test_gpio(dut):
    dut.expect_exact("Button test")
    dut.expect_exact("Button pressed 1 times")
    dut.expect_exact("Button pressed 2 times")
    dut.expect_exact("Button pressed 3 times")
