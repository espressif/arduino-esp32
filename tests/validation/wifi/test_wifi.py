import logging


def test_wifi(dut):
    LOGGER = logging.getLogger(__name__)

    LOGGER.info("Starting WiFi Scan")
    dut.expect_exact("Scan start")
    dut.expect_exact("Scan done")
    dut.expect_exact("Wokwi-GUEST")
    LOGGER.info("WiFi Scan done")

    LOGGER.info("Connecting to WiFi")
    dut.expect_exact("WiFi connected")
    dut.expect_exact("IP address:")
    LOGGER.info("WiFi connected")
