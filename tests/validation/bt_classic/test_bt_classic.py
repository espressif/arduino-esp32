"""
BT Classic (BluetoothSerial / SPP) hardware validation.

Two DUTs run sequentially through eight phases:
  1. basic_lifecycle     — begin / deinit / reinit on both devices
  2. spp_connect_data    — client discovers server by name, connects, exchanges
                           data bidirectionally, then disconnects
  3. bond_management     — both devices list bonds, delete all, verify empty
  4. discovery           — discover(), discoverAsync() and discoverStop()
  5. ssp_passkey_entry   — display/request passkey callbacks, passkey pairing
  6. ssp_numeric_compare — numeric-comparison confirm callbacks, pairing
  7. ssp_passkey_reject  — a rejected passkey fails authentication promptly
  8. memory_release      — end(releaseMemory=true) on both devices

Phase orchestration uses the same START_PHASE_N / "[ROLE] Phase N started"
serial protocol as the BLE validation tests.
"""

import logging
import time

from conftest import rand_str4

LOGGER = logging.getLogger(__name__)

PHASE_LABELS = {
    1: "basic_lifecycle",
    2: "spp_connect_data",
    3: "bond_management",
    4: "discovery",
    5: "ssp_passkey_entry",
    6: "ssp_numeric_compare",
    7: "ssp_passkey_reject",
    8: "memory_release",
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

    # External Bluedroid owner: BLE and Classic must adopt it and leave it up
    # when each library ends.
    server.expect(r"\[SERVER\] ExtHost ble=1 bleKeep=1 spp=1 sppKeep=1", timeout=30)
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
    # onConnect fires from callback as soon as the link is up (may precede Client connected)
    server.expect_exact("[SERVER] onConnect:", timeout=60)
    server.expect_exact("[SERVER] Client connected", timeout=10)
    server.expect_exact("[SERVER] peerCount=1", timeout=10)
    server.expect_exact("[SERVER] peers=1", timeout=10)
    server.expect(r"\[SERVER\] peerAddr=[0-9a-fA-F:]+", timeout=10)

    client.expect_exact("[CLIENT] peerCount=1", timeout=10)
    client.expect_exact("[CLIENT] peers=1", timeout=10)
    client.expect(r"\[CLIENT\] peerAddr=[0-9a-fA-F:]+", timeout=10)

    client.expect_exact("[CLIENT] Sent: SPP_WRITE_TO_PROBE", timeout=10)
    server.expect_exact("[SERVER] onPeerData:", timeout=15)
    server.expect_exact("[SERVER] writeTo ack sent", timeout=10)
    client.expect_exact("[CLIENT] Received: SPP_WRITE_TO_ACK", timeout=15)

    client.expect_exact("[CLIENT] Sent: HELLO_FROM_CLIENT", timeout=10)
    server.expect_exact("[SERVER] Received: HELLO_FROM_CLIENT", timeout=15)
    server.expect_exact("[SERVER] Sent: HELLO_FROM_SERVER", timeout=10)
    client.expect_exact("[CLIENT] Received: HELLO_FROM_SERVER", timeout=15)

    # BLE and Classic up at the same time: BLE must adopt the Bluedroid host that
    # BluetoothSerial started, and BLE.end() must not tear it down for the SPP
    # owner (the SPP link has to survive the BLE teardown below).
    client.expect(r"\[CLIENT\] Coex ble=1 spp=1", timeout=60)
    server.expect(r"\[SERVER\] Coex ble=1 adv=1 connected=1 spp=1", timeout=60)
    server.expect_exact("[SERVER] Coex teardown spp=1", timeout=20)
    client.expect_exact("[CLIENT] Coex teardown spp=1", timeout=20)
    server.expect(r"\[SERVER\] Coex adopt ble=1 spp=1", timeout=20)
    client.expect(r"\[CLIENT\] Coex adopt ble=1 spp=1", timeout=20)
    server.expect_exact("[SERVER] Coex adopt teardown spp=1", timeout=20)
    client.expect_exact("[CLIENT] Coex adopt teardown spp=1", timeout=20)
    # end(true) while Classic still owns the host: the shared host must stay up
    # and the BLE library must be terminally released.
    server.expect(r"\[SERVER\] Coex endtrue begin=1 keep=1 again=0", timeout=20)
    client.expect(r"\[CLIENT\] Coex endtrue begin=1 keep=1 again=0", timeout=20)

    client.expect_exact("[CLIENT] Sent: DISCONNECT_ME", timeout=10)
    # The callback may run before or after disconnect(addr) returns; the server
    # prints one summary line once both are observable.
    server.expect(r"\[SERVER\] disconnect done peerCount=0 callback=1", timeout=20)
    client.expect_exact("[CLIENT] Disconnected by server", timeout=15)
    server.expect_exact("[SERVER] Client disconnected", timeout=10)

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
    # Phase 4: discovery APIs
    # -------------------------------------------------------------------------
    _start_phase(server, client, 4)

    server.expect_exact("[SERVER] Discovery ready", timeout=10)
    client.expect_exact("[CLIENT] Discovering...", timeout=10)

    m = client.expect(r"\[CLIENT\] Discover found=(\d+) count=(\d+) attempts=\d+", timeout=40)
    assert int(m.group(1)) == 1, "blocking discover() did not find the server"
    assert int(m.group(2)) >= 1, "blocking discover() returned no devices"

    client.expect(r"\[CLIENT\] DiscoverAsync started=1 seen=1 attempts=\d+ results=\d+", timeout=60)
    client.expect_exact("[CLIENT] DiscoverStop=OK", timeout=10)

    record_property(f"phase_4_{PHASE_LABELS[4]}", "PASS")
    LOGGER.info("Phase 4 (%s) passed", PHASE_LABELS[4])

    # -------------------------------------------------------------------------
    # Phase 5: SSP passkey entry
    # -------------------------------------------------------------------------
    _start_phase(server, client, 5)

    server.expect_exact("[SERVER] Passkey begin=1", timeout=20)
    client.expect_exact("[CLIENT] Passkey begin=1", timeout=20)
    client.expect_exact("[CLIENT] Connecting (passkey)...", timeout=10)

    # The server is DisplayOnly: the stack generates the code and prints it.
    m = server.expect(r"\[SERVER\] Passkey=(\d{6}) peer=", timeout=60)
    passkey = int(m.group(1))
    client.expect(r"\[CLIENT\] Passkey requested by .* reject=0", timeout=60)

    # The server's code is the only valid value; feed it to the KeyboardOnly side.
    client.write(f"PASSKEY={passkey:06d}\n")
    client.expect_exact(f"[CLIENT] Passkey={passkey:06d}", timeout=10)
    client.expect_exact("[CLIENT] Passkey connect=OK", timeout=30)
    server.expect_exact("[SERVER] Passkey connected=1", timeout=30)

    server.expect_exact("[SERVER] Passkey probe=1", timeout=30)
    client.expect_exact("[CLIENT] Received: PASSKEY_ACK", timeout=30)

    client.expect_exact("[CLIENT] Passkey done connected=0 auth=1", timeout=30)
    server.expect_exact("[SERVER] Passkey done connected=0 auth=1", timeout=30)

    record_property(f"phase_5_{PHASE_LABELS[5]}", "PASS")
    LOGGER.info("Phase 5 (%s) passed", PHASE_LABELS[5])

    # -------------------------------------------------------------------------
    # Phase 6: SSP numeric comparison
    # -------------------------------------------------------------------------
    _start_phase(server, client, 6)

    server.expect_exact("[SERVER] Numeric begin=1", timeout=20)
    client.expect_exact("[CLIENT] Numeric begin=1", timeout=20)
    client.expect_exact("[CLIENT] Connecting (numeric)...", timeout=10)

    server.expect(r"\[SERVER\] Confirm=\d{6} accept=1", timeout=60)
    client.expect(r"\[CLIENT\] Confirm=\d{6} accept=1", timeout=60)

    client.expect_exact("[CLIENT] Numeric connect=OK", timeout=30)
    server.expect_exact("[SERVER] Numeric connected=1", timeout=30)
    server.expect_exact("[SERVER] Numeric probe=1", timeout=30)
    client.expect_exact("[CLIENT] Received: NUMERIC_ACK", timeout=30)

    client.expect_exact("[CLIENT] Numeric done connected=0 auth=1", timeout=30)
    server.expect_exact("[SERVER] Numeric done connected=0 auth=1", timeout=30)

    record_property(f"phase_6_{PHASE_LABELS[6]}", "PASS")
    LOGGER.info("Phase 6 (%s) passed", PHASE_LABELS[6])

    # -------------------------------------------------------------------------
    # Phase 7: SSP passkey entry rejected by the keyboard side
    # -------------------------------------------------------------------------
    _start_phase(server, client, 7)

    server.expect_exact("[SERVER] Reject begin=1", timeout=20)
    client.expect_exact("[CLIENT] Reject begin=1", timeout=20)
    client.expect_exact("[CLIENT] Connecting (reject)...", timeout=10)

    client.expect(r"\[CLIENT\] Passkey requested by .* reject=1", timeout=60)
    # The rejection is answered as soon as the request arrives. It must not wait
    # for the ~30 s authentication timeout, so authentication failure has to be
    # reported almost immediately.
    reject_start = time.monotonic()
    client.expect_exact("[CLIENT] Passkey rejected", timeout=10)
    server.expect_exact("[SERVER] Reject authFail=1 connected=0", timeout=10)
    reject_elapsed = time.monotonic() - reject_start
    assert reject_elapsed < 5.0, f"rejected passkey took {reject_elapsed:.1f}s to fail authentication"

    m = client.expect(r"\[CLIENT\] Reject connect=(\S+) elapsed=(\d+)", timeout=30)
    assert m.group(1).decode() != "OK", "connect() succeeded although the passkey was rejected"

    record_property(f"phase_7_{PHASE_LABELS[7]}", "PASS")
    LOGGER.info("Phase 7 (%s) passed", PHASE_LABELS[7])

    # -------------------------------------------------------------------------
    # Phase 8: memory release
    # -------------------------------------------------------------------------
    _start_phase(server, client, 8)

    server.expect_exact("[SERVER] Memory released", timeout=15)
    client.expect_exact("[CLIENT] Memory released", timeout=15)

    record_property(f"phase_8_{PHASE_LABELS[8]}", "PASS")
    LOGGER.info("Phase 8 (%s) passed", PHASE_LABELS[8])

    LOGGER.info("BT Classic validation test passed!")
