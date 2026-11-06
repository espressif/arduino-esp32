import logging


def test_power_management(dut):
    LOGGER = logging.getLogger(__name__)

    LOGGER.info("Phase 0: Running initial boot tests (reset reason, TWDT, light sleep)")
    dut.expect_unity_test_output(timeout=60)

    LOGGER.info("Waiting for deep sleep entry...")
    dut.expect_exact("DEEP_SLEEP_START", timeout=30)

    LOGGER.info("Phase 1: Waiting for deep sleep wakeup and RTC verification")
    dut.expect_unity_test_output(timeout=30)
