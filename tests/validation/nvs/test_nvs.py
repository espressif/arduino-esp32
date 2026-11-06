import logging


def test_nvs(dut):
    LOGGER = logging.getLogger(__name__)

    LOGGER.info("Phase 1: Running NVS unit tests + writing persistence data")
    dut.expect_unity_test_output(timeout=120)

    if not hasattr(dut, "serial") or dut.serial is None:
        LOGGER.info("Phase 2: Skipped (no serial port, e.g. Wokwi)")
        return

    LOGGER.info("Phase 2: Restarting to verify persistence")
    dut.serial.hard_reset()
    dut.expect_unity_test_output(timeout=60)
