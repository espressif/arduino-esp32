import logging


def test_gpio(dut):
    LOGGER = logging.getLogger(__name__)

    dut.expect_exact("Button test")

    LOGGER.info("Expecting button press 1")
    dut.expect_exact("Button pressed 1 times")

    LOGGER.info("Expecting button press 2")
    dut.expect_exact("Button pressed 2 times")

    LOGGER.info("Expecting button press 3")
    dut.expect_exact("Button pressed 3 times")
