import logging
from conftest import REGEX_IPV4, is_valid_ipv4, rand_str4


def test_webserver(dut, ci_job_id):
    LOGGER = logging.getLogger(__name__)

    server = dut[0]
    client = dut[1]

    ap_ssid = "WS_SSID_" + ci_job_id if ci_job_id else "WS_SSID_" + rand_str4()
    ap_password = "WS_PW_" + ci_job_id if ci_job_id else "WS_PW_" + rand_str4()

    LOGGER.info(f"AP SSID: {ap_ssid}")
    LOGGER.info(f"AP Password: {ap_password}")

    # Wait for devices to be ready
    LOGGER.info("Waiting for devices to be ready...")
    server.expect_exact("[SERVER] Device ready for WiFi credentials", timeout=120)
    client.expect_exact("[CLIENT] Device ready for WiFi credentials", timeout=120)

    # Send WiFi credentials to server
    server.expect_exact("[SERVER] Send SSID:")
    server.write(f"{ap_ssid}")
    server.expect_exact("[SERVER] Send Password:")
    server.write(f"{ap_password}")
    server.expect_exact(f"[SERVER] SSID: {ap_ssid}")
    server.expect_exact(f"[SERVER] Password: {ap_password}")
    LOGGER.info("Server credentials sent")

    # Wait for AP to start and get IP
    m = server.expect(rf"\[SERVER\] AP started IP={REGEX_IPV4}", timeout=20)
    server_ip = m.group(1).decode()
    LOGGER.info(f"Server AP started. IP: {server_ip}")
    assert is_valid_ipv4(server_ip)

    # Wait for server to be ready
    server.expect_exact("[SERVER] Server started", timeout=10)
    LOGGER.info("Server is ready")

    # Send WiFi credentials and server IP to client
    client.expect_exact("[CLIENT] Send SSID:")
    client.write(f"{ap_ssid}")
    client.expect_exact("[CLIENT] Send Password:")
    client.write(f"{ap_password}")
    client.expect_exact(f"[CLIENT] SSID: {ap_ssid}")
    client.expect_exact(f"[CLIENT] Password: {ap_password}")

    client.expect_exact("[CLIENT] Send server IP:")
    client.write(f"{server_ip}")
    client.expect_exact(f"[CLIENT] Server IP: {server_ip}")
    LOGGER.info("Client credentials and server IP sent")

    # Wait for client to connect
    m = client.expect(rf"\[CLIENT\] Connected IP={REGEX_IPV4}", timeout=30)
    client_ip = m.group(1).decode()
    LOGGER.info(f"Client connected. IP: {client_ip}")
    assert is_valid_ipv4(client_ip)

    # Verify each test result
    client.expect_exact("[CLIENT] PASS stream_text", timeout=10)
    LOGGER.info("PASS: stream_text")

    client.expect_exact("[CLIENT] PASS stream_binary", timeout=10)
    LOGGER.info("PASS: stream_binary")

    client.expect_exact("[CLIENT] PASS stream_explicit_len", timeout=10)
    LOGGER.info("PASS: stream_explicit_len")

    client.expect_exact("[CLIENT] PASS stream_empty", timeout=10)
    LOGGER.info("PASS: stream_empty")

    client.expect_exact("[CLIENT] PASS string", timeout=10)
    LOGGER.info("PASS: string")

    client.expect_exact("[CLIENT] All tests passed", timeout=10)
    LOGGER.info("All WebServer stream tests passed")
