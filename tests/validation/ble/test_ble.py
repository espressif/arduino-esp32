import re
import logging
from conftest import rand_str4


def test_ble(dut, ci_job_id):
    LOGGER = logging.getLogger(__name__)

    server = dut[0]
    client = dut[1]

    # Generate unique server name for this test run
    server_name = "BLE_SRV_" + ci_job_id if ci_job_id else "BLE_SRV_" + rand_str4()

    LOGGER.info(f"Server Name: {server_name}")

    # Wait for devices to be ready and send server name
    # Add longer timeout as the test starts running in one device while it is uploaded to the other
    LOGGER.info("Waiting for devices to be ready...")
    server.expect_exact("[SERVER] Device ready for server name", timeout=120)
    client.expect_exact("[CLIENT] Device ready for server name", timeout=120)

    server.expect_exact("[SERVER] Send server name:")
    client.expect_exact("[CLIENT] Send server name:")
    LOGGER.info(f"Sending server name: {server_name}")
    server.write(f"{server_name}")
    client.write(f"{server_name}")

    # Verify server name was received
    server.expect_exact(f"[SERVER] Server name: {server_name}")
    client.expect_exact(f"[CLIENT] Target server name: {server_name}")
    LOGGER.info("Server name received by both devices")

    # Wait for devices to initialize
    LOGGER.info("Waiting for devices to initialize...")
    server.expect_exact("[SERVER] Starting BLE Secure Server", timeout=30)
    client.expect_exact("[CLIENT] Starting BLE Secure Client", timeout=30)

    # Verify server is using correct BLE stack
    LOGGER.info("Checking BLE stack...")
    server.expect(r"\[SERVER\] BLE stack: (Bluedroid|NimBLE)", timeout=10)

    # Wait for server to be ready
    LOGGER.info("Waiting for server to start advertising...")
    server.expect_exact("[SERVER] Characteristics configured", timeout=10)
    server.expect_exact(f"[SERVER] Advertising started with name: {server_name}", timeout=10)
    LOGGER.info(f"Server advertising with name: {server_name}")

    m = server.expect_exact("[SERVER] Service UUID: 4fafc201-1fb5-459e-8fcc-c5c9c331914b", timeout=10)
    LOGGER.info(f"Server Service UUID: 4fafc201-1fb5-459e-8fcc-c5c9c331914b")

    # Wait for client to start scanning
    LOGGER.info("Waiting for client to start scanning...")
    client.expect_exact(f"[CLIENT] Scanning for server: {server_name}", timeout=10)

    # Client finds server
    LOGGER.info("Waiting for client to discover server...")
    client.expect_exact("[CLIENT] Found target server!", timeout=60)

    # Client connects to server
    LOGGER.info("Client connecting to server...")
    client.expect_exact("[CLIENT] Physical connection established", timeout=20)
    server.expect_exact("[SERVER] Client connected", timeout=10)

    # Verify service discovery starts
    LOGGER.info("Verifying service discovery...")
    client.expect_exact("[CLIENT] Found service", timeout=10)

    # Wait for numeric comparison PIN display on both devices
    # Note: Bluedroid (ESP32) initiates security on-connect, NimBLE initiates on-demand
    # So numeric comparison may happen during service discovery (Bluedroid) or later (NimBLE)
    LOGGER.info("Waiting for numeric comparison...")
    m_server = server.expect(r"\[SERVER\] Numeric comparison PIN: (\d+)", timeout=30)
    server_pin = m_server.group(1).decode()
    LOGGER.info(f"Server PIN: {server_pin}")
    server.expect_exact("[SERVER] Confirming PIN match", timeout=5)

    m_client = client.expect(r"\[CLIENT\] Numeric comparison PIN: (\d+)", timeout=30)
    client_pin = m_client.group(1).decode()
    LOGGER.info(f"Client PIN: {client_pin}")
    client.expect_exact("[CLIENT] Confirming PIN match", timeout=5)

    # Verify both devices show the same PIN
    assert server_pin == client_pin, f"PIN mismatch! Server: {server_pin}, Client: {client_pin}"
    LOGGER.info(f"PIN match confirmed: {server_pin}")

    # Continue with characteristics discovery
    client.expect_exact("[CLIENT] Found insecure characteristic", timeout=10)
    client.expect_exact("[CLIENT] Found secure characteristic", timeout=10)

    # Verify insecure characteristic read
    LOGGER.info("Verifying insecure characteristic access...")

    # Wait for authentication to complete on both devices
    LOGGER.info("Waiting for authentication to complete...")
    client.expect_exact("[CLIENT] Authentication complete", timeout=30)

    # Verify IRK retrieval on client side
    LOGGER.info("Verifying IRK retrieval on client...")
    client.expect(r"\[CLIENT\] Successfully retrieved peer IRK: ([0-9a-fA-F:]+)", timeout=10)

    # Server-side authentication may complete after characteristic read
    server.expect_exact("[SERVER] Characteristic read: beb5483e-36e1-4688-b7f5-ea07361b26a8", timeout=10)
    server.expect_exact("[SERVER] Authentication complete", timeout=30)

    # Verify IRK retrieval on server side
    LOGGER.info("Verifying IRK retrieval on server...")
    server.expect(r"\[SERVER\] Successfully retrieved peer IRK: ([0-9a-fA-F:]+)", timeout=10)

    client.expect_exact("[CLIENT] Insecure characteristic value: Insecure Hello World!", timeout=10)

    # Verify secure characteristic read
    LOGGER.info("Verifying secure characteristic access...")
    client.expect_exact("[CLIENT] Reading secure characteristic...", timeout=10)

    # Verify secure characteristic was read successfully
    server.expect_exact("[SERVER] Characteristic read: ff1d2614-e2d6-4c87-9154-6625d39ca7f8", timeout=10)
    client.expect_exact("[CLIENT] Secure characteristic value: Secure Hello World!", timeout=10)

    # Verify connection is established
    client.expect_exact("[CLIENT] Connection and authentication successful", timeout=10)
    client.expect_exact("[CLIENT] Successfully connected and authenticated", timeout=10)

    # Verify write and read operations on insecure characteristic
    LOGGER.info("Testing insecure characteristic write/read...")
    client.expect_exact("[CLIENT] Wrote to insecure characteristic: Test Insecure Write", timeout=10)
    server.expect_exact("[SERVER] Received write: Test Insecure Write", timeout=10)

    client.expect_exact("[CLIENT] Read from insecure characteristic: Test Insecure Write", timeout=10)
    server.expect_exact("[SERVER] Characteristic read: beb5483e-36e1-4688-b7f5-ea07361b26a8", timeout=10)
    LOGGER.info("Insecure characteristic write/read verified")

    # Verify write and read operations on secure characteristic
    LOGGER.info("Testing secure characteristic write/read...")
    client.expect_exact("[CLIENT] Wrote to secure characteristic: Test Secure Write", timeout=10)
    server.expect_exact("[SERVER] Received write: Test Secure Write", timeout=10)

    client.expect_exact("[CLIENT] Read from secure characteristic: Test Secure Write", timeout=10)
    server.expect_exact("[SERVER] Characteristic read: ff1d2614-e2d6-4c87-9154-6625d39ca7f8", timeout=10)
    LOGGER.info("Secure characteristic write/read verified")

    # Verify test completion
    client.expect_exact("[CLIENT] Test operations completed", timeout=10)
    LOGGER.info("All characteristic operations completed successfully")

    # Verify connection status
    LOGGER.info("Verifying connection status...")
    server.expect_exact("[SERVER] Status: Connected", timeout=5)

    LOGGER.info("BLE test passed!")

