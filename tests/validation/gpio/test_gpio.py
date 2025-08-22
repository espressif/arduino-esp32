import logging


def test_gpio(dut):
    dut.expect_exact("GPIO test END")
