import logging
import pytest


def test_networking(dut, wifi_ssid, wifi_pass):
    LOGGER = logging.getLogger(__name__)

    if not wifi_ssid:
        pytest.fail("WiFi SSID is required but not provided. Use --wifi-ssid argument.")

    LOGGER.info("Waiting for device to be ready...")
    dut.expect_exact("NET_READY")

    dut.expect_exact("Send SSID:")
    LOGGER.info(f"Sending WiFi credentials: SSID={wifi_ssid}")
    dut.write(f"{wifi_ssid}\n")

    dut.expect_exact("Send Password:")
    LOGGER.info("Sending WiFi password")
    dut.write(f"{wifi_pass or ''}\n")

    LOGGER.info("Running networking Unity tests")
    dut.expect_unity_test_output(timeout=180)
