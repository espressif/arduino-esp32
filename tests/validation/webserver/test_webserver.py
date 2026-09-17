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

    # Each test case is reported by the client as "PASS <name>" or "FAIL <name>".
    # The stream cases validate the send(Stream&) overload; the remaining cases
    # are regression checks for the request-parsing security reports and for the
    # legitimate request shapes those checks must keep accepting.
    test_cases = [
        # send(Stream&) overload coverage
        ("stream_text", 10),
        ("stream_binary", 10),
        ("stream_explicit_len", 10),
        ("stream_empty", 10),
        ("string", 10),
        # Request-parsing robustness / security regression checks
        ("auth_bypass", 15),  # report 2: bare Authorization username bypass
        ("path_traversal", 15),  # report 6: serveStatic dot-segment traversal
        # Compatibility checks for the limits added by the fixes above
        ("static_root", 15),  # serveStatic mapped to the filesystem root
        ("raw_body", 20),  # non-multipart body still streams to the callback
        ("many_args", 15),  # form with many fields still fully parsed
        ("long_uri", 20),  # long query served, over-long target answered 414
        ("long_line", 60),  # unterminated protocol line refused, long header accepted
        ("regex_route", 15),  # regex route still matches a normal-length path
        # Slow-client checks (upstream issue 12788)
        ("slow_line", 40),  # trickled bytes into an unterminated header line
        ("slow_headers", 50),  # endless stream of complete but tiny headers
        # Checks that crash or hang the server task on a vulnerable build
        ("upload_null_deref", 45),  # report 4: before multipart (masks null upload)
        ("arg_poison", 20),  # report 8: aborted multipart poisons later args
        ("arg_flood", 45),  # report 3: query-separator allocation amplification
        ("regex_stack", 60),  # report 7: UriRegex stack exhaustion
        ("regex_backtrack", 60),  # report 7: UriRegex catastrophic backtracking
        ("multipart_eof", 45),  # report 1: last — hangs server on vulnerable builds
    ]

    # Collect every case result (instead of stopping at the first failure) so a
    # run against a vulnerable build reports the full matrix of findings.
    failures = []
    for name, timeout in test_cases:
        m = client.expect(rf"\[CLIENT\] (PASS|FAIL) {name}", timeout=timeout)
        result = m.group(1).decode()
        if result == "PASS":
            LOGGER.info(f"PASS: {name}")
        else:
            LOGGER.error(f"FAIL: {name}")
            failures.append(name)

    assert not failures, f"WebServer test cases failed: {', '.join(failures)}"
    LOGGER.info("All WebServer tests passed")
