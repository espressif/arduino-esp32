import logging


def test_nvs(dut):
    LOGGER = logging.getLogger(__name__)

    LOGGER.info("Expecting counter value 0")
    dut.expect_exact("Current counter value: 0")

    LOGGER.info("Expecting counter value 1")
    dut.expect_exact("Current counter value: 1")

    LOGGER.info("Expecting counter value 2")
    dut.expect_exact("Current counter value: 2")
