import re
import logging
from conftest import REGEX_IPV4, is_valid_ipv4, rand_str4


def test_wifi_ap(dut, ci_job_id):
    LOGGER = logging.getLogger(__name__)

    ap = dut[0]
    client = dut[1]

    ap_ssid = "AP_SSID_" + ci_job_id if ci_job_id else "AP_SSID_" + rand_str4()
    ap_password = "AP_PW_" + ci_job_id if ci_job_id else "AP_PW_" + rand_str4()

    LOGGER.info(f"AP SSID: {ap_ssid}")
    LOGGER.info(f"AP Password: {ap_password}")

    # Wait for devices to be ready and send WiFi credentials
    # Add longer timeout as the test start running in one device while it is uploaded to the other
    LOGGER.info("Waiting for devices to be ready...")
    ap.expect_exact("[AP] Device ready for WiFi credentials", timeout=120)
    client.expect_exact("[CLIENT] Device ready for WiFi credentials", timeout=120)

    ap.expect_exact("[AP] Send SSID:")
    client.expect_exact("[CLIENT] Send SSID:")
    LOGGER.info(f"Sending WiFi credentials: SSID={ap_ssid}")
    ap.write(f"{ap_ssid}")
    client.write(f"{ap_ssid}")

    ap.expect_exact("[AP] Send Password:")
    client.expect_exact("[CLIENT] Send Password:")
    LOGGER.info(f"Sending WiFi password: Password={ap_password}")
    ap.write(f"{ap_password}")
    client.write(f"{ap_password}")

    # Verify credentials were received
    ap.expect_exact(f"[AP] SSID: {ap_ssid}")
    ap.expect_exact(f"[AP] Password: {ap_password}")
    client.expect_exact(f"[CLIENT] SSID: {ap_ssid}")
    client.expect_exact(f"[CLIENT] Password: {ap_password}")
    LOGGER.info(f"Credentials received")

    # Verify AP started
    LOGGER.info(f"Starting AP")
    m = ap.expect(rf"\[AP\] Started SSID={re.escape(ap_ssid)} Password={re.escape(ap_password)} IP={REGEX_IPV4}", timeout=20)
    ap_ip = m.group(1).decode()
    LOGGER.info(f"AP started successfully. IP: {ap_ip}")
    assert is_valid_ipv4(ap_ip)

    # Wait for AP to begin reporting station count
    LOGGER.info(f"Waiting for AP to begin reporting station count")
    ap.expect(r"\[AP\] Stations connected: \d+", timeout=20)

    # Client scans for AP
    LOGGER.info(f"Scanning for AP: {ap_ssid}")
    client.expect_exact("[CLIENT] Scan start")
    client.expect_exact("[CLIENT] Scan done")
    client.expect_exact(f"{ap_ssid}")
    LOGGER.info(f"AP found")

    # Client connects to AP
    LOGGER.info(f"Connecting to AP")
    client.expect_exact(f"[CLIENT] Connecting to SSID={ap_ssid} Password={ap_password}", timeout=20)

    # Client connection success
    m = client.expect(rf"\[CLIENT\] Connected! IP={REGEX_IPV4}", timeout=30)
    client_ip = m.group(1).decode()
    LOGGER.info(f"Client connection successful. IP: {client_ip}")
    assert is_valid_ipv4(client_ip)

    # Verify AP reports 1 station connected
    LOGGER.info(f"Waiting for AP to report 1 station connected")
    ap.expect_exact("[AP] Stations connected: 1", timeout=30)

    # Verify client stays connected
    LOGGER.info(f"Waiting for client to report connected status")
    client.expect_exact("[CLIENT] Status=3", timeout=10)  # WL_CONNECTED

    LOGGER.info(f"WiFi AP test passed")
