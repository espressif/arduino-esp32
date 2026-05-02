"""
BT Classic (BluetoothSerial / SPP) hardware validation.

Two DUTs run sequentially through four phases:
  1. basic_lifecycle  — begin / deinit / reinit on both devices
  2. spp_connect_data — client discovers server by name, connects, exchanges
                        data bidirectionally, then disconnects
  3. bond_management  — both devices list bonds, delete all, verify empty
  4. memory_release   — end(releaseMemory=true) on both devices

Phase orchestration uses the same START_PHASE_N / "[ROLE] Phase N started"
serial protocol as the BLE validation tests.
"""

import logging
from conftest import rand_str4

LOGGER = logging.getLogger(__name__)

PHASE_LABELS = {
    1: "basic_lifecycle",
    2: "spp_connect_data",
    3: "bond_management",
    4: "memory_release",
}


# ---------------------------------------------------------------------------
# Phase helpers
# ---------------------------------------------------------------------------


def _start_phase(server, client, phase_num):
    """Send phase start command to both devices and wait for acknowledgment."""
    LOGGER.info("Sending START_PHASE_%d to both devices", phase_num)
    server.write(f"START_PHASE_{phase_num}\n")
    client.write(f"START_PHASE_{phase_num}\n")
    server.expect_exact(f"[SERVER] Phase {phase_num} started", timeout=10)
    client.expect_exact(f"[CLIENT] Phase {phase_num} started", timeout=10)


# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------


def test_bt_classic(dut, ci_job_id, record_property):
    server = dut[0]
    client = dut[1]

    base = ci_job_id if ci_job_id else rand_str4()
    name = f"BTC_{base}"
    LOGGER.info("BT Classic server name: %s", name)

    # Exchange names with both devices before starting phases
    server.expect_exact("[SERVER] Send name:", timeout=30)
    client.expect_exact("[CLIENT] Send server name:", timeout=30)
    server.write(f"{name}\n")
    client.write(f"{name}\n")

    # -------------------------------------------------------------------------
    # Phase 1: basic lifecycle
    # -------------------------------------------------------------------------
    _start_phase(server, client, 1)

    server.expect(r"\[SERVER\] Init OK, addr: [0-9a-fA-F:]+", timeout=30)
    server.expect_exact("[SERVER] Deinit OK", timeout=10)
    server.expect_exact("[SERVER] Reinit OK", timeout=30)

    client.expect(r"\[CLIENT\] Init OK, addr: [0-9a-fA-F:]+", timeout=30)
    client.expect_exact("[CLIENT] Deinit OK", timeout=10)
    client.expect_exact("[CLIENT] Reinit OK", timeout=30)

    record_property(f"phase_1_{PHASE_LABELS[1]}", "PASS")
    LOGGER.info("Phase 1 (%s) passed", PHASE_LABELS[1])

    # -------------------------------------------------------------------------
    # Phase 2: SPP connect + data exchange
    # -------------------------------------------------------------------------
    _start_phase(server, client, 2)

    server.expect_exact("[SERVER] Waiting for client...", timeout=10)
    client.expect_exact("[CLIENT] Connecting...", timeout=10)

    # BT inquiry + SPP connect may take up to ~30 s
    client.expect_exact("[CLIENT] Connected", timeout=60)
    server.expect_exact("[SERVER] Client connected", timeout=60)

    client.expect_exact("[CLIENT] Sent: HELLO_FROM_CLIENT", timeout=10)
    server.expect_exact("[SERVER] Received: HELLO_FROM_CLIENT", timeout=15)
    server.expect_exact("[SERVER] Sent: HELLO_FROM_SERVER", timeout=10)
    client.expect_exact("[CLIENT] Received: HELLO_FROM_SERVER", timeout=15)

    client.expect_exact("[CLIENT] Disconnected", timeout=10)
    server.expect_exact("[SERVER] Client disconnected", timeout=15)

    record_property(f"phase_2_{PHASE_LABELS[2]}", "PASS")
    LOGGER.info("Phase 2 (%s) passed", PHASE_LABELS[2])

    # -------------------------------------------------------------------------
    # Phase 3: bond management
    # -------------------------------------------------------------------------
    _start_phase(server, client, 3)

    server.expect(r"\[SERVER\] Bonds: \d+", timeout=10)
    server.expect_exact("[SERVER] DeleteAllBonds: OK", timeout=10)
    server.expect_exact("[SERVER] Bonds after delete: 0", timeout=10)

    client.expect(r"\[CLIENT\] Bonds: \d+", timeout=10)
    client.expect_exact("[CLIENT] DeleteAllBonds: OK", timeout=10)
    client.expect_exact("[CLIENT] Bonds after delete: 0", timeout=10)

    record_property(f"phase_3_{PHASE_LABELS[3]}", "PASS")
    LOGGER.info("Phase 3 (%s) passed", PHASE_LABELS[3])

    # -------------------------------------------------------------------------
    # Phase 4: memory release
    # -------------------------------------------------------------------------
    _start_phase(server, client, 4)

    server.expect_exact("[SERVER] Memory released", timeout=15)
    client.expect_exact("[CLIENT] Memory released", timeout=15)

    record_property(f"phase_4_{PHASE_LABELS[4]}", "PASS")
    LOGGER.info("Phase 4 (%s) passed", PHASE_LABELS[4])

    LOGGER.info("BT Classic validation test passed!")
