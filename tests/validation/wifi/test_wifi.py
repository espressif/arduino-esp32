import logging
import pytest


def test_wifi(dut, wifi_ssid, wifi_pass):
    LOGGER = logging.getLogger(__name__)

    # Fail if no WiFi SSID is provided
    if not wifi_ssid:
        pytest.fail("WiFi SSID is required but not provided. Use --wifi-ssid argument.")

    # Wait for device to be ready and send WiFi credentials
    LOGGER.info("Waiting for device to be ready...")
    dut.expect_exact("Device ready for WiFi credentials")

    dut.expect_exact("Send SSID:")
    LOGGER.info(f"Sending WiFi credentials: SSID={wifi_ssid}")
    dut.write(f"{wifi_ssid}\n")

    dut.expect_exact("Send Password:")
    LOGGER.info(f"Sending WiFi password: Password={wifi_pass}")
    dut.write(f"{wifi_pass or ''}\n")

    # Verify credentials were received
    dut.expect_exact(f"SSID: {wifi_ssid}")
    dut.expect_exact(f"Password: {wifi_pass or ''}")

    LOGGER.info("Starting WiFi Scan")
    dut.expect_exact("Scan start")
    dut.expect_exact("Scan done")

    LOGGER.info(f"Looking for WiFi network: {wifi_ssid}")
    dut.expect_exact(wifi_ssid)
    LOGGER.info("WiFi Scan done")

    LOGGER.info("Connecting to WiFi")
    dut.expect_exact("WiFi connected")
    dut.expect_exact("IP address:")
    LOGGER.info("WiFi connected")
