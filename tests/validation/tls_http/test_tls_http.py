import logging
import pytest


def _get_server_ca_cert(hostname="postman-echo.com", port=443):
    """Fetch the root CA certificate for a given host at test time."""
    import ssl
    import socket
    import base64

    ctx = ssl.create_default_context()
    with ctx.wrap_socket(socket.socket(), server_hostname=hostname) as s:
        s.settimeout(10)
        s.connect((hostname, port))
        chain = s.get_verified_chain()

    if not chain:
        raise RuntimeError("Could not retrieve certificate chain")

    # The last certificate in the verified chain is the root CA
    root_der = chain[-1]
    b64 = base64.encodebytes(root_der).decode()
    return f"-----BEGIN CERTIFICATE-----\n{b64}-----END CERTIFICATE-----\n"


def test_tls_http(dut, wifi_ssid, wifi_pass):
    LOGGER = logging.getLogger(__name__)

    if not wifi_ssid:
        pytest.fail("WiFi SSID is required but not provided. Use --wifi-ssid argument.")

    LOGGER.info("Waiting for device to be ready...")
    dut.expect_exact("TLS_HTTP_READY")

    dut.expect_exact("Send SSID:")
    LOGGER.info(f"Sending WiFi credentials: SSID={wifi_ssid}")
    dut.write(f"{wifi_ssid}\n")

    dut.expect_exact("Send Password:")
    LOGGER.info("Sending WiFi password")
    dut.write(f"{wifi_pass or ''}\n")

    dut.expect_exact("SEND_CA_CERT")
    LOGGER.info("Fetching CA cert for postman-echo.com and sending to DUT")
    try:
        pem = _get_server_ca_cert()
    except Exception as e:
        pytest.fail(f"Failed to fetch CA cert for postman-echo.com: {e}")
    for line in pem.strip().splitlines():
        dut.write(f"{line}\n")
    dut.write("CERT_END\n")

    dut.expect(r"GOT_CERT len=\d+", timeout=10)
    LOGGER.info("Running TLS/HTTP Unity tests")
    dut.expect_unity_test_output(timeout=180)
