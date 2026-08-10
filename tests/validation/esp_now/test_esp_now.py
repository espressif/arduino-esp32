"""
ESP-NOW multi-DUT validation test.

device0 = master (tests/validation/esp_now/master)
device1 = slave  (tests/validation/esp_now/slave)

Test phases
-----------
1  Init, pre-begin edge cases, master broadcast (onNewPeer)
2  Slave broadcast + recv_bcast flag verification
3  Unicast master → slave
4  Unicast slave → master
5  Peer-management getters + addr()/setRate() edge cases
   (between 5 and 6: setKey printed before "Ready for phase 6")
6  Encrypted unicast master → slave
7  Maximum-length payload
8  Peer remove / re-add
9  end() / begin() lifecycle + ESP_NOW.write()
"""

import logging
import re

LOGGER = logging.getLogger(__name__)


def start_phase(master, slave, phase, timeout=60):
    """Synchronize both DUTs into a phase.

    Slave receives START before master to avoid race conditions where the
    master starts sending before the slave has set up its receive state.
    """
    LOGGER.info("Waiting for devices ready: phase %d", phase)
    slave.expect_exact(f"[SLAVE] Ready for phase {phase}", timeout=timeout)
    master.expect_exact(f"[MASTER] Ready for phase {phase}", timeout=timeout)
    LOGGER.info("Starting phase %d", phase)
    slave.write("START")
    slave.expect_exact(f"[SLAVE] Phase {phase} started", timeout=10)
    master.write("START")
    master.expect_exact(f"[MASTER] Phase {phase} started", timeout=10)
    LOGGER.info("Phase %d active on both devices", phase)


def test_esp_now(dut):
    master = dut[0]
    slave = dut[1]

    # ------------------------------------------------------------------ #
    # Phase 0 – Startup synchronization                                   #
    # Block until both DUTs have booted and are waiting, then release     #
    # them together regardless of flash-time differences.                 #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 0: Startup synchronization ===")
    start_phase(master, slave, 0, timeout=120)

    # ------------------------------------------------------------------ #
    # MAC exchange                                                         #
    # ------------------------------------------------------------------ #
    LOGGER.info("Exchanging MAC addresses")

    master_mac_match = master.expect(r"\[MASTER\] MAC: ([0-9A-Fa-f:]+)", timeout=30)
    master_mac = master_mac_match.group(1).decode()
    LOGGER.info("Master MAC: %s", master_mac)

    slave_mac_match = slave.expect(r"\[SLAVE\] MAC: ([0-9A-Fa-f:]+)", timeout=30)
    slave_mac = slave_mac_match.group(1).decode()
    LOGGER.info("Slave MAC: %s", slave_mac)

    master.expect_exact("[MASTER] Send peer MAC:", timeout=10)
    master.write(slave_mac)
    master.expect(rf"\[MASTER\] Peer MAC: {re.escape(slave_mac)}", timeout=10)

    slave.expect_exact("[SLAVE] Send peer MAC:", timeout=10)
    slave.write(master_mac)
    slave.expect(rf"\[SLAVE\] Peer MAC: {re.escape(master_mac)}", timeout=10)

    # ------------------------------------------------------------------ #
    # Phase 1 – Init, pre-begin edge cases, broadcast                     #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 1: Init and broadcast ===")

    # Pre-begin edge cases: all three getters must return -1.
    master.expect_exact("[MASTER] getVersion before begin: -1", timeout=30)
    master.expect_exact("[MASTER] getMaxDataLen before begin: -1", timeout=10)
    master.expect_exact("[MASTER] getTotalPeerCount before begin: -1", timeout=10)

    # After begin(), version must be ≥ 1 and maxDataLen must be 250 or 1470.
    version_match = master.expect(r"\[MASTER\] Version: (\d+)", timeout=15)
    version = int(version_match.group(1).decode())
    assert version >= 1, f"Unexpected ESP-NOW version: {version}"

    maxlen_match = master.expect(r"\[MASTER\] Max data len: (\d+)", timeout=10)
    max_data_len = int(maxlen_match.group(1).decode())
    assert max_data_len in (250, 1470), f"Unexpected max data len: {max_data_len}"

    # availableForWrite() must equal getMaxDataLen().
    avail_match = master.expect(r"\[MASTER\] availableForWrite: (\d+)", timeout=10)
    assert int(avail_match.group(1).decode()) == max_data_len

    slave.expect(r"\[SLAVE\] Version: (\d+)", timeout=30)
    slave.expect(r"\[SLAVE\] Max data len: (\d+)", timeout=10)

    start_phase(master, slave, 1)

    master.expect_exact("[MASTER] Broadcast sent: success", timeout=15)
    slave.expect_exact("[SLAVE] Received broadcast: Hello broadcast", timeout=15)
    slave.expect_exact("[SLAVE] master_peer bool (added): true", timeout=10)

    # ------------------------------------------------------------------ #
    # Phase 2 – Slave broadcast + recv_bcast flag verification            #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 2: Slave broadcast + recv_bcast flag ===")
    start_phase(master, slave, 2)

    # Slave sends broadcast; master's slave_peer.onReceive fires with broadcast=true.
    slave.expect_exact("[SLAVE] Broadcast sent: success", timeout=15)
    master.expect_exact("[MASTER] Received slave broadcast: Hello from slave bcast", timeout=15)
    master.expect_exact("[MASTER] Received as broadcast: true", timeout=10)

    # ------------------------------------------------------------------ #
    # Phase 3 – Unicast master → slave                                    #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 3: Unicast master → slave ===")
    start_phase(master, slave, 3)

    master.expect_exact("[MASTER] Unicast to slave: success", timeout=15)
    slave.expect_exact("[SLAVE] Received unicast: Phase3:M->S", timeout=15)

    # ------------------------------------------------------------------ #
    # Phase 4 – Unicast slave → master                                    #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 4: Unicast slave → master ===")
    start_phase(master, slave, 4)

    slave.expect_exact("[SLAVE] Unicast to master: success", timeout=15)
    master.expect_exact("[MASTER] Received from slave: Phase4:S->M", timeout=15)

    # ------------------------------------------------------------------ #
    # Phase 5 – Peer management                                           #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 5: Peer management ===")
    start_phase(master, slave, 5)

    # Master: broadcast_peer + slave_peer = 2 total, 0 encrypted.
    master.expect_exact("[MASTER] Total peers: 2", timeout=15)
    master.expect_exact("[MASTER] Encrypted peers: 0", timeout=10)
    master.expect_exact("[MASTER] Slave channel: 1", timeout=10)
    master.expect_exact("[MASTER] Slave interface: 0", timeout=10)
    master.expect_exact("[MASTER] Slave isEncrypted: false", timeout=10)
    master.expect_exact("[MASTER] slave_peer bool (added): true", timeout=10)

    # addr() getter must return the slave's MAC.
    slave_addr_match = master.expect(r"\[MASTER\] Slave addr: ([0-9A-Fa-f:]+)", timeout=10)
    assert slave_addr_match.group(1).decode().upper() == slave_mac.upper()

    # addr() setter and setRate() must fail while peer is added and running.
    master.expect_exact("[MASTER] addr() set while added: failed", timeout=10)
    master.expect_exact("[MASTER] Slave addr unchanged: true", timeout=10)
    master.expect_exact("[MASTER] setRate() while added: failed", timeout=10)

    # Slave: master_peer (phase 1) + bcast_peer (phase 2) = 2 total, 0 encrypted.
    slave.expect_exact("[SLAVE] Total peers: 2", timeout=15)
    slave.expect_exact("[SLAVE] Encrypted peers: 0", timeout=10)
    slave.expect_exact("[SLAVE] Master channel: 1", timeout=10)
    slave.expect_exact("[SLAVE] Master interface: 0", timeout=10)
    slave.expect_exact("[SLAVE] Master isEncrypted: false", timeout=10)
    slave.expect_exact("[SLAVE] master_peer bool (added): true", timeout=10)

    master_addr_match = slave.expect(r"\[SLAVE\] Master addr: ([0-9A-Fa-f:]+)", timeout=10)
    assert master_addr_match.group(1).decode().upper() == master_mac.upper()

    slave.expect_exact("[SLAVE] addr() set while added: failed", timeout=10)
    slave.expect_exact("[SLAVE] Master addr unchanged: true", timeout=10)
    slave.expect_exact("[SLAVE] setRate() while added: failed", timeout=10)

    # setKey outputs appear between phases 5 and 6 (before "Ready for phase 6").
    master.expect_exact("[MASTER] isEncrypted after setKey: true", timeout=10)
    master.expect_exact("[MASTER] Encrypted peers after setKey: 1", timeout=10)
    slave.expect_exact("[SLAVE] isEncrypted after setKey: true", timeout=10)
    slave.expect_exact("[SLAVE] Encrypted peers after setKey: 1", timeout=10)

    # ------------------------------------------------------------------ #
    # Phase 6 – Encrypted unicast                                         #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 6: Encrypted unicast ===")
    start_phase(master, slave, 6)

    master.expect_exact("[MASTER] Encrypted message sent: success", timeout=15)
    slave.expect_exact("[SLAVE] Received encrypted: EncryptedMsg", timeout=15)

    # ------------------------------------------------------------------ #
    # Phase 7 – Maximum-length payload                                    #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 7: Maximum-length payload ===")
    start_phase(master, slave, 7)

    sent_match = master.expect(r"\[MASTER\] Large payload sent: (\d+) bytes \(success\)", timeout=15)
    sent_len = int(sent_match.group(1).decode())
    assert sent_len == max_data_len, f"Expected {max_data_len} bytes, master sent {sent_len}"

    recv_match = slave.expect(r"\[SLAVE\] Large payload received: (\d+) bytes", timeout=15)
    recv_len = int(recv_match.group(1).decode())
    assert recv_len == max_data_len, f"Expected {max_data_len} bytes, slave received {recv_len}"

    slave.expect_exact("[SLAVE] Large payload pattern: ok", timeout=10)

    # ------------------------------------------------------------------ #
    # Phase 8 – Peer remove / re-add                                      #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 8: Peer remove / re-add ===")
    start_phase(master, slave, 8)

    # operator bool() must be false immediately after remove.
    master.expect_exact("[MASTER] slave_peer bool (removed): false", timeout=15)
    # Only broadcast_peer remains.
    master.expect_exact("[MASTER] Total peers after remove: 1", timeout=10)
    # After re-add, count returns to 2 (still encrypted since LMK was retained).
    master.expect_exact("[MASTER] Total peers after re-add: 2", timeout=10)
    master.expect_exact("[MASTER] Send after re-add: success", timeout=15)
    slave.expect_exact("[SLAVE] Received after re-add: AfterReAdd", timeout=15)

    # ------------------------------------------------------------------ #
    # Phase 9 – end() / begin() lifecycle + ESP_NOW.write()              #
    # ------------------------------------------------------------------ #
    LOGGER.info("=== Phase 9: end() / begin() lifecycle + ESP_NOW.write() ===")

    # end/begin happen before the phase handshake; expect them before start_phase.
    master.expect_exact("[MASTER] end(): success", timeout=15)
    master.expect_exact("[MASTER] begin() after end: success", timeout=15)
    slave.expect_exact("[SLAVE] end(): success", timeout=15)
    slave.expect_exact("[SLAVE] begin() after end: success", timeout=15)

    start_phase(master, slave, 9)

    # ESP_NOW.write() sends to all registered peers (only slave_peer after reinit).
    write_bytes_match = master.expect(r"\[MASTER\] ESP_NOW\.write\(\) bytes: (\d+)", timeout=15)
    assert int(write_bytes_match.group(1).decode()) > 0
    master.expect_exact("[MASTER] ESP_NOW.write() sent: success", timeout=15)
    slave.expect_exact("[SLAVE] Received after reinit: WriteToAll", timeout=15)

    master.expect_exact("[MASTER] Test complete", timeout=15)
    slave.expect_exact("[SLAVE] Test complete", timeout=15)

    LOGGER.info("All phases passed")
