"""
Combined BLE hardware validation.

Phases run sequentially on two DUTs (server + client).
Single test function, one upload; each phase recorded via record_property
so sub-results appear individually in the final report.
"""

import logging

import pytest

from conftest import rand_str4

LOGGER = logging.getLogger(__name__)

PHASE_LABELS = {
    1: "basic_lifecycle",
    2: "ble5_ext_periodic_adv",
    3: "gatt_setup_connect",
    4: "gatt_read_write",
    5: "notifications_indications",
    6: "large_att_write",
    7: "descriptor_read_write",
    8: "write_no_response",
    9: "server_disconnect",
    10: "security",
    11: "ble5_phy_dle",
    12: "reconnect",
    13: "blestream",
    14: "l2cap_coc",
    15: "introspection_and_permissions",
    16: "conninfo_and_params",
    17: "address_types",
    18: "authorization",
    19: "encrypted_perm_enforcement",
    20: "adv_data_and_scan",
    21: "beacon_and_eddystone",
    22: "bond_and_whitelist",
    23: "error_paths_and_misc",
    24: "ble5_advanced",
    25: "hid_smoke",
    26: "ble5_legacy_over_ext",
    27: "ble5_legacy_plus_ext",
    28: "l2cap_bulk_reconnect",
    29: "conn_params_reject",
    30: "deletebond_roundtrip",
    31: "nc_reject",
    32: "passkey_entry",
    33: "indication_busy_retry",
    34: "periodic_adv_lifecycle",
    35: "pending_send_edges",
    36: "directed_adv_and_oob",
    37: "memory_release",
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


def _phase_basic(server, client):
    # Heap canaries around the very first begin(): a stack init that overruns a
    # block corrupts the heap silently and only panics later, somewhere else.
    for dut, tag in ((server, "SERVER"), (client, "CLIENT")):
        m = dut.expect(rf"\[{tag}\] Phase1 heapIntegrity before=([01]) after=([01])", timeout=30)
        assert int(m.group(1)) == 1, f"{tag} heap was already corrupt before BLE.begin()"
        assert int(m.group(2)) == 1, f"{tag} BLE.begin() corrupted the heap"

    server.expect_exact("[SERVER] Init OK", timeout=30)
    server.expect_exact("[SERVER] Bare lifecycle OK", timeout=30)
    m_s_end = server.expect(
        r"\[SERVER\] Phase1 deferredEnd during=([01]) failSpawn=([01]) idleTrue=([01]) afterIdle=([01]) heap=([01])",
        timeout=30,
    )
    assert int(m_s_end.group(1)) == 1, "callback-context BLE.end() allowed begin() before teardown"
    assert int(m_s_end.group(2)) == 1, "failed ble-end worker left begin() able to revive the stack"
    assert int(m_s_end.group(3)) == 1, "idle end(true) after end(false) blocked re-begin"
    assert int(m_s_end.group(4)) == 1, "idle end(true) latched into a later end(false) and released controller memory"
    assert int(m_s_end.group(5)) == 1, "SERVER heap corrupted around deferred BLE.end()"
    server.expect_exact("[SERVER] Deinit OK", timeout=10)
    server.expect_exact("[SERVER] Reinit OK", timeout=30)
    server.expect_exact("[SERVER] Final deinit OK", timeout=10)

    client.expect_exact("[CLIENT] Init OK", timeout=30)
    client.expect_exact("[CLIENT] Bare lifecycle OK", timeout=30)
    m_c_end = client.expect(
        r"\[CLIENT\] Phase1 deferredEnd during=([01]) failSpawn=([01]) idleTrue=([01]) afterIdle=([01]) heap=([01])",
        timeout=30,
    )
    assert int(m_c_end.group(1)) == 1, "callback-context BLE.end() allowed begin() before teardown"
    assert int(m_c_end.group(2)) == 1, "failed ble-end worker left begin() able to revive the stack"
    assert int(m_c_end.group(3)) == 1, "idle end(true) after end(false) blocked re-begin"
    assert int(m_c_end.group(4)) == 1, "idle end(true) latched into a later end(false) and released controller memory"
    assert int(m_c_end.group(5)) == 1, "CLIENT heap corrupted around deferred BLE.end()"
    client.expect_exact(
        "[CLIENT] Blocking scan shutdown task=1 active=1 released=1 cancelFallback=1 admission=1 heap=1",
        timeout=15,
    )
    client.expect_exact("[CLIENT] Deinit OK", timeout=10)
    client.expect_exact("[CLIENT] Reinit OK", timeout=30)
    client.expect_exact("[CLIENT] Final deinit OK", timeout=10)


def _assert_phase2_clear_and_reset(server, timeout):
    m_busy = server.expect(
        r"\[SERVER\] Phase2 clearWhileLive ok=([01]) stillAdv=([01])",
        timeout=timeout,
    )
    assert (
        int(m_busy.group(1)) == 0 and int(m_busy.group(2)) == 1
    ), "clearExtended() while advertising must Fail and leave isAdvertising true"
    m_clr = server.expect(
        r"\[SERVER\] Phase2 clearThenRestart clear=([01]) idle=([01]) restart=([01])",
        timeout=timeout,
    )
    assert (
        int(m_clr.group(1)) == 1 and int(m_clr.group(2)) == 1 and int(m_clr.group(3)) == 1
    ), "clearExtended() then startExtended() did not leave idle then restart"
    m_reset = server.expect(
        r"\[SERVER\] Phase2 resetWhileExt idle=([01]) restart=([01])",
        timeout=timeout,
    )
    assert (
        int(m_reset.group(1)) == 1 and int(m_reset.group(2)) == 1
    ), "reset() while extended+periodic advertising did not leave idle then restart"
    m_stale = server.expect(
        r"\[SERVER\] Phase2 staleStopFlag start=([01]) stillAdv=([01]) stop=([01]) "
        r"idle=([01]) clear=([01]) skip=([01])",
        timeout=timeout,
    )
    if int(m_stale.group(6)) == 0:
        assert (
            int(m_stale.group(1)) == 1
            and int(m_stale.group(2)) == 1
            and int(m_stale.group(3)) == 1
            and int(m_stale.group(4)) == 1
            and int(m_stale.group(5)) == 1
        ), (
            "leftover EXT_ADV_STOP_COMPLETE after start() must not clear isAdvertising "
            "(stop() would no-op while the radio is up)"
        )
    m_per = server.expect(
        r"\[SERVER\] Phase2 stalePeriodicStop start=([01]) stillPer=([01]) skip=([01])",
        timeout=timeout,
    )
    if int(m_per.group(3)) == 0:
        assert int(m_per.group(1)) == 1 and int(m_per.group(2)) == 1, (
            "leftover PERIODIC_ADV_STOP_COMPLETE after startPeriodicAdv() must not " "clear periodicAdvRunning"
        )


def _phase_ble5_adv(server, client):
    """Returns (ble5_server, ble5_client) booleans."""
    m_server = server.expect(r"\[SERVER\] (BLE5 init OK|BLE5 not supported, skipping)", timeout=30)
    ble5_server = b"BLE5 init OK" in m_server.group(0)

    m_client = client.expect(r"\[CLIENT\] (BLE5 init OK|BLE5 not supported, skipping)", timeout=30)
    ble5_client = b"BLE5 init OK" in m_client.group(0)

    if ble5_server and ble5_client:
        server.expect_exact("[SERVER] Extended adv configured", timeout=10)
        m_dur = server.expect(r"\[SERVER\] Phase2 durationInvalid=([01])", timeout=10)
        assert (
            int(m_dur.group(1)) == 1
        ), "startExtended() duration that overflows 16-bit 10 ms units must be InvalidParam"
        server.expect_exact("[SERVER] Extended adv started", timeout=10)
        server.expect_exact("[SERVER] Periodic adv started", timeout=10)

        client.expect_exact("[CLIENT] Scanning for periodic adv...", timeout=10)
        client.expect_exact("[CLIENT] Found target via ext scan!", timeout=60)
        client.expect_exact("[CLIENT] Synced to periodic adv!", timeout=60)
        client.expect_exact("[CLIENT] Periodic data received", timeout=30)
        client.expect_exact("[CLIENT] Periodic test complete", timeout=10)

        _assert_phase2_clear_and_reset(server, timeout=30)
        server.expect_exact("[SERVER] BLE5 adv phase done", timeout=30)
        client.expect_exact("[CLIENT] BLE5 scan phase done", timeout=10)
    else:
        # Mixed-capability pair, e.g. a BLE5-capable P4 against an ESP32. The
        # capable side still runs its full advertising or scanning window while
        # the other returns at once. Wait for it here: the phase barrier below
        # would otherwise release phase 3 while that side is still advertising,
        # and the peer would scan for a server that cannot answer yet.
        if ble5_server:
            _assert_phase2_clear_and_reset(server, timeout=60)
            server.expect_exact("[SERVER] BLE5 adv phase done", timeout=30)
        if ble5_client:
            client.expect_exact("[CLIENT] BLE5 scan phase done", timeout=60)

    return ble5_server and ble5_client


def _phase_gatt_setup_connect(server, client):
    server.expect(r"\[SERVER\] Heap before init: \d+", timeout=30)
    server.expect_exact("[SERVER] GATT init OK", timeout=30)
    server.expect(r"\[SERVER\] Heap after init: \d+", timeout=10)
    server.expect_exact("[SERVER] Security configured", timeout=10)
    server.expect_exact("[SERVER] Services after begin: 0", timeout=10)
    server.expect_exact("[SERVER] Const lookups ok=1", timeout=10)
    server.expect_exact("[SERVER] Const lookup handle writable=1", timeout=10)
    server.expect(r"\[SERVER\] Heap after server: \d+", timeout=10)
    server.expect_exact("[SERVER] Server started", timeout=10)
    # Nobody can be connected before advertising starts.
    server.expect_exact("[SERVER] No-client notify status=ERROR_NO_CLIENT onNotify=0", timeout=10)
    server.expect_exact("[SERVER] No-client indicate status=ERROR_NO_CLIENT onNotify=0", timeout=10)
    server.expect_exact("[SERVER] Advertising started", timeout=10)

    client.expect(r"\[CLIENT\] Heap before init: \d+", timeout=30)
    client.expect_exact("[CLIENT] GATT init OK", timeout=30)
    client.expect(r"\[CLIENT\] Heap after init: \d+", timeout=10)
    client.expect(r"\[CLIENT\] Scanning \(attempt \d+\)\.\.\.", timeout=10)
    client.expect_exact("[CLIENT] Found target server!", timeout=60)
    client.expect_exact("[CLIENT] Connected", timeout=20)
    m_cache = client.expect(r"\[CLIENT\] ScanCacheInCallback ok=([01])", timeout=10)
    assert int(m_cache.group(1)) == 1, "scan results were not cached before onResult ran"
    m_order = client.expect(r"\[CLIENT\] ConnectCallbackBeforeReturn ok=([01])", timeout=10)
    assert int(m_order.group(1)) == 1, "onConnect had not fired when blocking connect() returned"
    server.expect_exact("[SERVER] Client connected", timeout=10)

    client.expect(r"\[CLIENT\] Connect time: \d+ ms", timeout=10)
    client.expect(r"\[CLIENT\] Negotiated MTU: \d+", timeout=10)
    client.expect(r"\[CLIENT\] Heap after connect: \d+", timeout=10)
    client.expect_exact("[CLIENT] Found service", timeout=10)


def _phase_gatt_rw(server, client):
    client.expect_exact("[CLIENT] Found characteristic", timeout=10)

    client.expect_exact("[CLIENT] Read value: Hello from server!", timeout=10)
    client.expect(r"\[CLIENT\] Read latency: \d+ us", timeout=10)
    client.expect_exact("[CLIENT] Write OK", timeout=10)
    server.expect_exact("[SERVER] Received write: Hello from client!", timeout=10)
    client.expect_exact("[CLIENT] Read-back: Hello from client!", timeout=10)

    # The server's onRead handler calls setValue() on the handle it receives, so
    # each read must return the value produced for that read.
    client.expect_exact("[CLIENT] Dynamic read 1: dyn_chr_1", timeout=10)
    client.expect_exact("[CLIENT] Dynamic read 2: dyn_chr_2", timeout=10)
    client.expect_exact("[CLIENT] Dynamic cached: dyn_chr_2", timeout=10)

    # 512 bytes do not fit one read response, so the client follows up with Read
    # Blob. onRead must run once per read: a value stitched from several onRead
    # calls is not uniform, and an extra call shifts the second read past 'B'.
    _expect_long_reads(client, "Long read", "AB")
    m_bnd = client.expect(
        r"\[CLIENT\] Long read boundary: len=(\d+) expected=(\d+) gen=(\S) uniform=([01])",
        timeout=15,
    )
    assert int(m_bnd.group(1)) == int(
        m_bnd.group(2)
    ), f"Exact (MTU-1) long read returned {int(m_bnd.group(1))} bytes, expected {int(m_bnd.group(2))}"
    assert m_bnd.group(3).decode() == "C", f"Exact (MTU-1) long read gen={m_bnd.group(3).decode()}, expected 'C'"
    assert int(m_bnd.group(4)) == 1, "Exact (MTU-1) long read was corrupted by concurrent setValue()"

    # The server rewrites this value every few ms, so without a per-connection
    # copy the Read Blob would return the tail of a newer value.
    for index in range(1, 11):
        m = client.expect(rf"\[CLIENT\] Snapshot read {index}: len=(\d+) gen=\S uniform=([01])", timeout=15)
        assert int(m.group(1)) == 512, f"Snapshot read {index} returned {int(m.group(1))} of 512 bytes"
        assert int(m.group(2)) == 1, f"Snapshot read {index} mixed two values set during one long read"

    client.expect_exact("[CLIENT] Const lookup read: set_via_const_lookup", timeout=10)
    client.expect_exact("[CLIENT] Concurrent GATT reads=1 writes=1 commands=1", timeout=120)

    # A long read dropped by disconnect must fail cleanly; the reconnect
    # re-read must be a uniform 512-byte snapshot. Two BLEClient instances on
    # one board share a single ACL with this peripheral, so overlapping
    # cross-client snapshots need a third DUT.
    m_ab = client.expect(r"\[CLIENT\] Snapshot abandon empty=([01])", timeout=15)
    assert int(m_ab.group(1)) == 1, "abandoned long read returned a value instead of failing"
    m_rec = client.expect(r"\[CLIENT\] Snapshot abandon reconnect uniform=([01])", timeout=20)
    assert int(m_rec.group(1)) == 1, "re-read after abandoned snapshot was not a uniform 512-byte value"


def _expect_long_reads(client, label, generations):
    for index, gen in enumerate(generations, start=1):
        m = client.expect(rf"\[CLIENT\] {label} {index}: len=(\d+) gen=(\S) uniform=([01])", timeout=15)
        assert int(m.group(1)) == 512, f"{label} {index} returned {int(m.group(1))} of 512 bytes"
        assert int(m.group(3)) == 1, f"{label} {index} mixed values from several onRead calls"
        got = m.group(2).decode()
        assert got == gen, f"{label} {index} came from onRead call '{got}', expected '{gen}'"


def _phase_notifications(server, client):
    client.expect_exact("[CLIENT] Subscribed to notifications", timeout=10)
    client.expect_exact("[CLIENT] Notification received: notify_test_1", timeout=30)
    client.expect_exact("[CLIENT] Malformed CCCD rejected: 1", timeout=10)
    client.expect_exact("[CLIENT] Subscribed to indications", timeout=10)
    client.expect_exact("[CLIENT] Indication received: indicate_test_1", timeout=30)
    # Sent back to back: ATT allows one unconfirmed indication per client, so
    # the server must queue them and deliver all of them, in order.
    m = client.expect(r"\[CLIENT\] Indication burst: (\S*)\r?\n", timeout=20)
    burst = m.group(1).decode()
    assert burst == "burst_1,burst_2,burst_3,burst_4", f"indication burst delivered '{burst}'"
    # One characteristic with both kinds enabled; the server interleaves them,
    # each notification sent while an indication is still unconfirmed.
    m = client.expect(r"\[CLIENT\] Dual indications: (\S*)\r?\n", timeout=20)
    dual_ind = m.group(1).decode()
    m = client.expect(r"\[CLIENT\] Dual notifications: (\S*)\r?\n", timeout=5)
    dual_notif = m.group(1).decode()
    assert dual_ind == "di_1,di_2,di_3,di_4", f"dual characteristic delivered indications '{dual_ind}'"
    assert dual_notif == "dn_1,dn_2,dn_3,dn_4", f"dual characteristic delivered notifications '{dual_notif}'"
    client.expect_exact("[CLIENT] Unsubscribed", timeout=15)

    # "Subscriber count: 1" may appear before phase expectations due to callback timing.
    # Keep the phase robust by validating sent events and final unsubscribe state.
    server.expect_exact("[SERVER] Notification sent", timeout=10)
    # The client received "notify_test_1" (set by onNotify) rather than the
    # "notify_stale" value set before notify(), so onNotify ran before the send.
    server.expect_exact("[SERVER] Notify status=SUCCESS_NOTIFY code=0 onNotify=1", timeout=10)
    server.expect_exact("[SERVER] Indication sent", timeout=10)
    server.expect_exact("[SERVER] Indicate status=SUCCESS_INDICATE code=0 onNotify=1", timeout=10)
    server.expect_exact("[SERVER] Indicate burst accepted=4", timeout=10)
    server.expect_exact("[SERVER] Indicate burst confirmed=4 failed=0", timeout=15)
    m_ov = server.expect(
        r"\[SERVER\] Indicate overflow accepted=(\d+) busy=(\d+) confirmed=(\d+) failed=(\d+)\r?\n",
        timeout=20,
    )
    ov_accepted, ov_busy = int(m_ov.group(1)), int(m_ov.group(2))
    ov_confirmed, ov_failed = int(m_ov.group(3)), int(m_ov.group(4))
    assert ov_accepted >= 8, f"indication overflow accepted {ov_accepted}, queue holds 8 behind the in-flight one"
    assert ov_busy >= 1, "indication overflow never returned Busy (queue-full untested)"
    assert ov_confirmed == ov_accepted, f"queued overflow indications confirmed={ov_confirmed} accepted={ov_accepted}"
    assert ov_failed == ov_busy, f"overflow Busy count {ov_busy} did not match failed statuses {ov_failed}"
    # Notifications and indications on one characteristic complete through the
    # same Bluedroid event; each status must be reported as the right kind.
    server.expect_exact("[SERVER] Dual confirmed ind=4 notif=4 failed=0", timeout=15)
    server.expect_exact("[SERVER] Subscriber count: 0", timeout=10)


def _phase_large_write(server, client):
    server.expect_exact("[SERVER] No-subscriber notify status=ERROR_NO_SUBSCRIBER onNotify=0 subEvents=2", timeout=10)
    client.expect_exact("[CLIENT] Large write OK", timeout=30)
    server.expect_exact("[SERVER] Received 512 bytes", timeout=10)
    client.expect_exact("[CLIENT] Large read (512 bytes)", timeout=30)
    client.expect_exact("[CLIENT] Large data integrity OK", timeout=10)
    client.expect_exact("[CLIENT] Shorter long write OK", timeout=30)
    server.expect_exact("[SERVER] Received 510 bytes", timeout=10)
    client.expect_exact("[CLIENT] Shorter long read (510 bytes)", timeout=30)
    client.expect_exact("[CLIENT] Shorter long data integrity OK", timeout=10)


def _phase_descriptor_read_write(server, client):
    client.expect_exact("[CLIENT] Found descriptor char", timeout=10)
    client.expect_exact("[CLIENT] Descriptor count: 6", timeout=10)
    client.expect_exact("[CLIENT] User description: Test Characteristic", timeout=10)
    client.expect_exact("[CLIENT] Presentation format: 25 exp=-2 unit=0x2700 ns=1 desc=0x0042", timeout=10)
    client.expect_exact("[CLIENT] Format matches UTF8", timeout=10)
    client.expect_exact("[CLIENT] Dynamic descriptor read 1: dyn_desc_1", timeout=10)
    client.expect_exact("[CLIENT] Dynamic descriptor read 2: dyn_desc_2", timeout=10)
    _expect_long_reads(client, "Long descriptor read", "ab")
    server.expect_exact("[SERVER] Descriptor write count=1 len=13", timeout=10)
    client.expect_exact("[CLIENT] Writable descriptor short write ok=1 readback=desc_short_ok", timeout=10)
    server.expect_exact("[SERVER] Descriptor write count=2 len=510", timeout=20)
    client.expect_exact("[CLIENT] Writable descriptor long write ok=1 len=510 uniform=1", timeout=20)
    client.expect_exact("[CLIENT] Writable descriptor oversize rejected=1 rollbackLen=510 rollbackOk=1", timeout=20)


def _phase_write_no_response(server, client):
    client.expect_exact("[CLIENT] Found WriteNR char", timeout=10)
    client.expect_exact("[CLIENT] WriteNR sent", timeout=10)
    server.expect_exact("[SERVER] WriteNR received: WriteNR_OK", timeout=10)
    # Use [^\r\n]+ not .+ : on CRLF, "." matches \r and greedy .+ can swallow the next line.
    client.expect(r"\[CLIENT\] WriteNR readback: [^\r\n]+", timeout=10)

    # Burst of three queued Write-Without-Response packets. A value buffer shared
    # across queued packets makes every onWrite() read the newest payload, so the
    # server would report CCCCCC three times instead of AA/BB/CC.
    m_sent = client.expect(r"\[CLIENT\] Phase8 burst sent=([01])", timeout=15)
    assert int(m_sent.group(1)) == 1, "client failed to send the Write-NR burst"
    for index, payload in enumerate(("AAAAAA", "BBBBBB", "CCCCCC"), start=1):
        m = server.expect(rf"\[SERVER\] Phase8 burst {index} bytes=([0-9A-F]{{6}}) ok=([01])", timeout=15)
        got = m.group(1).decode()
        assert got == payload, f"Write-NR packet {index} arrived as {got}, expected {payload}"
        assert int(m.group(2)) == 1, f"server rejected Write-NR packet {index}"
    m_done = server.expect(r"\[SERVER\] Phase8 burst done seen=(\d+) ok=([01])", timeout=15)
    assert int(m_done.group(1)) == 3, f"server saw {int(m_done.group(1))} of 3 Write-NR burst packets"
    assert int(m_done.group(2)) == 1, "Write-NR burst payloads were not preserved"

    client.expect_exact("[CLIENT] Status: write_no_response done", timeout=10)


def _phase_server_disconnect(server, client):
    client.expect_exact("[CLIENT] Waiting for server disconnect...", timeout=10)
    server.expect_exact("[SERVER] Client disconnected", timeout=10)
    server.expect_exact("[SERVER] Server-initiated disconnect OK", timeout=10)
    client.expect_exact("[CLIENT] Server disconnected us", timeout=15)
    m_td = client.expect(r"\[CLIENT\] DisconnectTornDownInCallback ok=([01])", timeout=10)
    assert int(m_td.group(1)) == 1, "connection state was not torn down before onDisconnect ran"
    client.expect_exact("[CLIENT] Reconnected after server disconnect", timeout=30)
    server.expect_exact("[SERVER] Client connected", timeout=10)


def _phase_security(server, client):
    m_server = server.expect(r"\[SERVER\] Passkey: (\d+)\r?\n", timeout=30)
    server_pin = m_server.group(1).decode()
    m_client = client.expect(r"\[CLIENT\] Passkey: (\d+)\r?\n", timeout=30)
    client_pin = m_client.group(1).decode()
    assert server_pin == client_pin, f"PIN mismatch: {server_pin} vs {client_pin}"

    server.expect_exact("[SERVER] Authentication complete", timeout=30)
    client.expect_exact("[CLIENT] Authentication complete", timeout=30)
    client.expect_exact("[CLIENT] Secure read: Secure Data!", timeout=10)
    # onIdentity dedupe: the peer identity is surfaced exactly once per
    # connection on both backends. NimBLE receives identity from both
    # IDENTITY_RESOLVED and ENC_CHANGE and dedupes it in the library; Bluedroid
    # fires it once from AUTH_CMPL. A count != 1 means the dedupe regressed.
    m_id = client.expect(r"\[CLIENT\] Phase10 onIdentity count=(\d+)\r?\n", timeout=10)
    assert int(m_id.group(1)) == 1, f"onIdentity fired {m_id.group(1)} times, expected exactly 1 (dedupe regressed)"

    # Cross-stack event-order parity: the client lifecycle callbacks for a fresh
    # pairing connection must fire in the SAME order on NimBLE (esp32s3) and
    # Bluedroid (esp32). The golden order per impl/CONTRACT.md is
    # onConnect -> onMtuChanged -> onIdentity. Asserting the identical sequence on
    # both backends is what proves parity (the same test runs on both targets).
    #
    # Use expect_exact on the full golden string — not a greedy ([A-Z,]+) capture.
    # Serial arrives in arbitrary chunks; a prefix-matching regex can succeed on
    # "CONNE" before "CT,MTU,IDENTITY" arrives. An exact full-line payload cannot.
    client.expect_exact("[CLIENT] Phase10 ORDER=CONNECT,MTU,IDENTITY", timeout=10)


def _phase_ble5_phy_dle(client):
    """Returns True if BLE5 PHY/DLE ran, False if skipped."""
    m = client.expect(
        r"\[CLIENT\] (PHY update OK|BLE5 PHY/DLE not supported, skipping)",
        timeout=10,
    )
    if b"PHY update OK" in m.group(0):
        client.expect_exact("[CLIENT] DLE set OK", timeout=10)
        return True
    return False


def _phase_reconnect(server, client):
    client.expect_exact("[CLIENT] Disconnected for reconnect test", timeout=10)
    server.expect_exact("[SERVER] Client disconnected", timeout=10)

    for i in range(3):
        client.expect_exact(f"[CLIENT] Connect cycle {i + 1}", timeout=30)
        client.expect_exact("[CLIENT] Connected", timeout=20)
        client.expect_exact("[CLIENT] Disconnected", timeout=10)

    client.expect_exact("[CLIENT] All cycles complete", timeout=10)
    # Server callbacks can race relative to client log order; require both events
    # at least once instead of per-cycle strict alternation.
    server.expect_exact("[SERVER] Client connected", timeout=10)
    server.expect_exact("[SERVER] Client disconnected", timeout=10)

    # App ID boundary stress test (Bluedroid-only)
    m = client.expect(
        r"\[CLIENT\] AppId stress (PASSED|SKIPPED \(NimBLE\)|FAILED.*)",
        timeout=120,
    )
    if b"FAILED" in m.group(0):
        raise RuntimeError(f"App ID stress test failed: {m.group(0).decode()}")
    m_ref = client.expect(
        r"\[CLIENT\] Phase12 nimbleRef race supported=([01]) clean=([01]) "
        r"emptyOnConnect=([01]) resetWhileConnected=([01]) reconnect=([01]) connected=([01])",
        timeout=30,
    )
    assert int(m_ref.group(2)) == 1, "NimBLE client keepalive was reset across reconnect"


def _phase_blestream(server, client):
    # Init + onConnect callback + connected()
    server.expect_exact("[SERVER] BLEStream malformed NUS ready=1", timeout=20)
    server.expect_exact("[SERVER] BLEStream existing NUS rejected=1", timeout=15)
    client.expect_exact("[CLIENT] BLEStream malformed begin armed", timeout=15)
    client.write("PHASE13_MALFORMED_GO\n")
    client.expect_exact("[CLIENT] BLEStream failed begin events connect=1 disconnect=1", timeout=20)
    client.expect_exact("[CLIENT] BLEStream failed begin cleanup=1", timeout=20)
    server.write("PHASE13_CONTINUE\n")
    server.expect_exact("[SERVER] BLEStream malformed NUS removed=1", timeout=15)
    server.expect_exact("[SERVER] BLEStream init OK", timeout=20)
    client.expect_exact("[CLIENT] BLEStream init OK", timeout=30)
    server.expect_exact("[SERVER] BLEStream onConnect fired", timeout=15)
    server.expect_exact("[SERVER] BLEStream callback mutation safe=1", timeout=15)
    server.expect_exact("[SERVER] BLEStream connected OK", timeout=10)
    server.expect_exact("[SERVER] BLEStream peerCount=1", timeout=10)
    server.expect_exact("[SERVER] BLEStream peers=1", timeout=10)
    server.expect(r"\[SERVER\] BLEStream peerAddr=[0-9a-fA-F:]+", timeout=10)
    client.expect_exact("[CLIENT] BLEStream connected OK", timeout=10)
    client.expect_exact("[CLIENT] BLEStream peers=1", timeout=10)
    client.expect(r"\[CLIENT\] BLEStream peerAddr=[0-9a-fA-F:]+", timeout=10)

    # writeTo + onData round-trip
    client.expect_exact("[CLIENT] BLEStream writeTo probe sent", timeout=10)
    server.expect_exact("[SERVER] BLEStream onPeerData:", timeout=15)
    server.expect_exact("[SERVER] BLEStream writeTo ack sent", timeout=10)
    client.expect_exact("[CLIENT] BLEStream onPeerData:", timeout=15)
    client.expect_exact("[CLIENT] BLEStream writeTo ack OK", timeout=15)

    # Basic ping/pong (client → server → client)
    client.expect_exact("[CLIENT] BLEStream sent", timeout=10)
    server.expect_exact("[SERVER] BLEStream received: stream_ping", timeout=15)
    server.expect_exact("[SERVER] BLEStream reply sent", timeout=10)
    client.expect_exact("[CLIENT] BLEStream received: STREAM_PONG", timeout=15)

    # peek() test: server peeks first byte ('p' = 112) then reads the line
    client.expect_exact("[CLIENT] BLEStream peek_test sent", timeout=10)
    server.expect_exact("[SERVER] BLEStream peek: 112", timeout=15)
    server.expect_exact("[SERVER] BLEStream peek read: peek_test", timeout=10)

    # Bulk write: 200-byte A-Z pattern (multi-MTU chunking + integrity)
    client.expect_exact("[CLIENT] BLEStream bulk sent: 200 bytes returned=202", timeout=10)
    server.expect_exact("[SERVER] BLEStream bulk received: 200 bytes", timeout=15)
    server.expect_exact("[SERVER] BLEStream bulk integrity OK", timeout=10)

    # Server-initiated write (server → client direction)
    server.expect_exact("[SERVER] BLEStream server msg sent", timeout=10)
    client.expect_exact("[CLIENT] BLEStream server msg: server_says_hi", timeout=15)

    # Server end() owns the active teardown: callback fires before return,
    # advertising remains stopped, and the client observes the disconnect.
    server.expect_exact("[SERVER] BLEStream onDisconnect fired=1", timeout=15)
    server.expect_exact("[SERVER] BLEStream end advertisingStopped=1", timeout=15)
    server.expect_exact("[SERVER] BLEStream rebegin ok=1", timeout=15)
    server.expect_exact("[SERVER] BLEStream reentrant end=1 concurrentWriteRejected=1 heap=1", timeout=15)
    client.expect_exact("[CLIENT] BLEStream server end disconnected=1", timeout=15)
    m_handle = client.expect(
        r"\[CLIENT\] BLEStream handle reset race clean=([01]) sawCleared=([01]) entered=([01])",
        timeout=15,
    )
    assert (
        int(m_handle.group(1)) == 1 and int(m_handle.group(2)) == 0 and int(m_handle.group(3)) == 1
    ), "BLEStream writeChunks observed a handle cleared by concurrent end()"
    client.expect_exact("[CLIENT] BLEStream callback end cycle connected=1 disconnects=1", timeout=30)
    server.expect_exact("[SERVER] BLEStream callback end returned=1 rebegin=1 heap=1", timeout=15)
    server.expect_exact("[SERVER] BLEStream final owner ready=1", timeout=15)
    m_final_client = client.expect(
        r"\[CLIENT\] BLEStream final owner callback clean=([01]) connects=(\d+) disconnects=(\d+)",
        timeout=30,
    )
    assert int(m_final_client.group(1)) == 1
    assert int(m_final_client.group(2)) > 0 and int(m_final_client.group(3)) > 0
    server.expect_exact(
        "[SERVER] BLEStream final owner dropped=1 recoveryBlocked=1 workerFails=0 teardownFails=0 rebegin=1 heap=1",
        timeout=20,
    )
    server.expect_exact("[SERVER] BLEStream partial start failed=1 recovery=1 removeFails=0 heap=1", timeout=20)
    client.expect_exact("[CLIENT] Status: blestream done", timeout=10)


def _phase_l2cap(server, client):
    """Returns True if L2CAP CoC ran, False if skipped."""
    m_server = server.expect(r"\[SERVER\] (L2CAP init OK|L2CAP not supported, skipping)", timeout=20)
    m_client = client.expect(r"\[CLIENT\] (L2CAP init OK|L2CAP not supported, skipping)", timeout=20)
    if b"L2CAP init OK" in m_server.group(0) and b"L2CAP init OK" in m_client.group(0):
        server.expect_exact("[SERVER] L2CAP channel accepted", timeout=15)
        client.expect_exact("[CLIENT] L2CAP channel connected", timeout=15)
        client.expect_exact("[CLIENT] L2CAP sent", timeout=10)
        server.expect_exact("[SERVER] L2CAP received: L2CAP_PING", timeout=15)
        server.expect_exact("[SERVER] L2CAP reply sent", timeout=10)
        m_host_busy = server.expect(r"\[SERVER\] L2CAP hostWriteBusy=([01])", timeout=10)
        assert (
            int(m_host_busy.group(1)) == 1
        ), "host-callback L2CAP write larger than one SDU must return Busy before sending"
        client.expect_exact("[CLIENT] L2CAP received: L2CAP_OK", timeout=15)
        client.expect_exact("[CLIENT] Status: l2cap done", timeout=10)
        return True
    return False


def _phase_introspection_and_permissions(server, client):
    """Phase 15 — exercises broader client API surface and verifies the new
    properties/permissions validation system for both characteristics and
    descriptors.

    Specifically asserts:
      * Services/characteristics can be enumerated from the client.
      * getRSSI() returns a value (connected link).
      * can*() introspection reflects the peer's declared properties on each
        known characteristic (including post-fail-closed masking).
      * A characteristic declared as Read|Write with only Read permission
        is advertised without the Write property and rejects writes at ATT.
      * The CCCD (0x2902) is present on a Notify characteristic, on both
        NimBLE (auto-created by the host) and Bluedroid (explicit).
      * bleValidateDescProps rejects malformed descriptor specs at
        construction time (None perms, reserved-UUID access-profile
        violations, authorization without a base direction) and accepts
        valid ones.
      * A read-only descriptor is readable and its write is rejected at ATT
        by both backends — regression guard for the historical Bluedroid
        bug where descriptor permissions were hardcoded to READ|WRITE.
    """
    # Both DUTs run in parallel. Consume the client's early reconnect marker
    # first so its subsequent introspection burst cannot push that marker out
    # of pytest-embedded's bounded expect window while server checks are read.
    client.expect_exact("[CLIENT] Reconnected for introspection", timeout=30)

    # Server-side construction-time validator checks.
    server.expect_exact("[SERVER] DescPerm validation start", timeout=15)
    server.expect_exact("[SERVER] DescPerm None rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2904+Write rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2900+Write rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2902 read-only rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2901 no-read rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm WriteAuthorized-no-write rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm valid accepted: 1", timeout=10)
    server.expect_exact("[SERVER] CharProp validation rejected=4/4 validAccepted=1", timeout=10)
    server.expect_exact("[SERVER] IncludedService removeBusy=1 removeOrderOk=1 removeInvalidOk=1", timeout=10)
    server.expect_exact("[SERVER] DescriptorHelpers ok=1", timeout=10)
    server.expect_exact("[SERVER] DescPerm validation done", timeout=10)

    m_rssi = client.expect(r"\[CLIENT\] RSSI: (-?\d+)\r?\n", timeout=10)
    rssi = int(m_rssi.group(1))
    # -128 is the sentinel getRSSI() returns when the read never completes, so
    # matching any integer here let a broken read pass as a success. Both DUTs
    # sit on the same bench, well inside this range.
    assert -110 <= rssi <= 20, f"getRSSI() returned {rssi}; -128 means the read failed or timed out"
    m_svc = client.expect(r"\[CLIENT\] Service count: (\d+)\r?\n", timeout=10)
    assert int(m_svc.group(1)) >= 1, "Expected at least 1 service on server"
    m_ch = client.expect(r"\[CLIENT\] Characteristic count: (\d+)\r?\n", timeout=10)
    # 6 original + 1 new fail-closed char. Allow >= to be backend-tolerant.
    assert int(m_ch.group(1)) >= 6, "Expected at least 6 characteristics on service"

    # Property introspection — exact flags match server-declared properties
    # (the RW char has only Read|Write, no Notify/Indicate, etc.).
    client.expect_exact("[CLIENT] RW: canRead=1 canWrite=1 canNotify=0 canIndicate=0", timeout=10)
    client.expect_exact("[CLIENT] Notify: canNotify=1 canIndicate=0 canRead=1", timeout=10)
    client.expect_exact("[CLIENT] Indicate: canIndicate=1 canNotify=0 canRead=1", timeout=10)
    client.expect_exact("[CLIENT] WriteNR: canWriteNoResponse=1 canWrite=0", timeout=10)

    # CCCD auto-created (NimBLE) or explicit (Bluedroid) — must be discoverable
    # from the client either way, and descriptor discovery must stay strictly
    # within each characteristic's handle range.
    client.expect_exact("[CLIENT] RW descriptor count: 0 noCccdNotFound=1", timeout=10)
    client.expect_exact("[CLIENT] Notify descriptor count: 1", timeout=10)
    client.expect_exact("[CLIENT] Notify CCCD found: 1", timeout=10)
    client.expect_exact("[CLIENT] Dual descriptor count: 2 userDesc=Dual Characteristic", timeout=10)

    # Fail-closed permission masking: Write property must be stripped, reads
    # must succeed, writes must be rejected at the ATT layer.
    client.expect_exact("[CLIENT] FailClosed: canRead=1 canWrite=0", timeout=10)
    client.expect_exact("[CLIENT] FailClosed read: ro_data", timeout=10)
    client.expect(r"\[CLIENT\] FailClosed write rejected: [^\r\n]+", timeout=10)

    # Runtime enforcement of read-only descriptor permissions. A write here
    # succeeding would mean the Bluedroid descPerm translation regressed
    # back to the hardcoded READ|WRITE behavior.
    client.expect_exact("[CLIENT] DescPerm RO descriptor found", timeout=10)
    client.expect_exact("[CLIENT] DescPerm RO read: ro_desc_val", timeout=10)
    client.expect(r"\[CLIENT\] DescPerm RO write rejected: [^\r\n]+", timeout=10)

    client.expect_exact("[CLIENT] Introspection done", timeout=15)


def _phase_conninfo_and_params(server, client):
    """Phase 16 — BLEConnInfo matrix + MTU/conn-params callbacks.

    Verifies both sides fire onMtuChanged/onConnParamsUpdate and that
    BLEConnInfo reports sane values (non-zero handle, interval within the
    controller range, encryption flags consistent with the existing bond).
    """
    client.expect_exact("[CLIENT] Phase16 connected", timeout=30)
    client.expect(r"\[CLIENT\] ConnInfo handle=\d+ mtu=\d+ clientMtu=\d+", timeout=10)
    client.expect(r"\[CLIENT\] ConnInfo peerAddr=\S+ handle=\d+", timeout=10)
    client.expect(r"\[CLIENT\] ConnInfo sec enc=[01] auth=[01] bond=[01] keySize=\d+", timeout=10)
    client.expect(r"\[CLIENT\] ConnInfo role central=[01] peripheral=[01]", timeout=10)
    client.expect(r"\[CLIENT\] ConnInfo params interval=\d+ latency=\d+ timeout=\d+", timeout=10)
    client.expect(r"\[CLIENT\] ConnInfo phy tx=\d+ rx=\d+", timeout=10)
    client.expect(r"\[CLIENT\] ConnInfo rssi=-?\d+", timeout=10)
    client.expect(r"\[CLIENT\] UpdateConnParams ok=[01]", timeout=10)
    # The client calls setMTU(247) before connecting and the server setMTU(512),
    # so an ATT MTU exchange MUST happen on every connection and the client's
    # onMtuChanged MUST fire with a negotiated MTU above the 23-byte default.
    # (Regression guard: on Bluedroid the request used to be issued from a GATTC
    # event callback and silently never delivered CFG_MTU_EVT.)
    m_mtu = client.expect(r"\[CLIENT\] MtuChanged seen=([01]) last=(\d+)\r?\n", timeout=10)
    assert int(m_mtu.group(1)) == 1, "client onMtuChanged never fired — MTU exchange did not complete"
    assert int(m_mtu.group(2)) == 247, f"client negotiated MTU {m_mtu.group(2)} != 247 (setMTU(247) ignored?)"
    # Ordering contract (impl/CONTRACT.md "Client MTU exchange"): onMtuChanged
    # must fire after onConnect on both backends.
    m_order = client.expect(r"\[CLIENT\] MtuOrder afterConnect=([01]) mtuSeen=([01])", timeout=10)
    assert int(m_order.group(2)) == 1, "client onMtuChanged never fired — MTU exchange did not complete"
    assert int(m_order.group(1)) == 1, "onMtuChanged fired before onConnect — MTU exchange ordering contract violated"
    client.expect_exact("[CLIENT] Phase16 done", timeout=10)

    server.expect_exact("[SERVER] Phase16 waiting for peer", timeout=15)
    server.expect(r"\[SERVER\] ConnInfo handle=\d+ mtu=\d+", timeout=30)
    server.expect(r"\[SERVER\] ConnInfo peerMtu=\d+", timeout=10)
    server.expect(r"\[SERVER\] ConnInfo sec enc=[01] auth=[01] bond=[01] keySize=\d+", timeout=10)
    server.expect(r"\[SERVER\] ConnInfo role central=[01] peripheral=[01]", timeout=10)
    server.expect(r"\[SERVER\] ConnInfo params interval=\d+ latency=\d+ timeout=\d+", timeout=10)
    server.expect(r"\[SERVER\] ConnInfo addr ota=\S+ id=\S+", timeout=10)
    m_smtu = server.expect(r"\[SERVER\] MtuChanged seen=([01]) last=(\d+)\r?\n", timeout=15)
    assert int(m_smtu.group(1)) == 1, "server onMtuChanged never fired — MTU exchange did not complete"
    assert int(m_smtu.group(2)) == 247, f"server negotiated MTU {m_smtu.group(2)} != 247 (setMTU(247) ignored?)"
    server.expect(r"\[SERVER\] ConnParamsUpdate seen=[01] interval=\d+ latency=\d+ timeout=\d+", timeout=10)
    # getConnInfo(handle) must return a fresh snapshot reflecting the post-exchange
    # MTU/conn-params, reject a bogus handle, and behave the same on both stacks.
    m_gci = server.expect(
        r"\[SERVER\] Phase16 getConnInfo valid=([01]) mtu=(\d+) timeout=(\d+) bogusValid=([01]) "
        r"refreshedMtu=([01]) refreshedParams=([01])",
        timeout=10,
    )
    assert int(m_gci.group(1)) == 1, "getConnInfo(handle) returned an invalid snapshot for a live connection"
    assert int(m_gci.group(4)) == 0, "getConnInfo(bogus handle) unexpectedly returned a valid snapshot"
    assert int(m_gci.group(5)) == 1, "getConnInfo did not reflect the refreshed (post-exchange) MTU"
    assert int(m_gci.group(6)) == 1, "getConnInfo did not reflect the refreshed connection parameters"
    server.expect_exact("[SERVER] Phase16 done", timeout=10)


def _phase_address_types(server, client):
    """Phase 17 — BTAddress::Type API surface + connection roundtrip per type.

    Verifies every BTAddress::Type enum value is accepted by
    BLE.setOwnAddressType and that the server can advertise + accept a
    connection using at least one of Public/Random. PublicID/RandomID are
    exercised for API surface only (they require LL privacy which is not
    guaranteed on every target).
    """
    for tag, dut in [("SERVER", server), ("CLIENT", client)]:
        dut.expect(rf"\[{tag}\] OwnAddress addr=\S+ type=\d+", timeout=15)
        dut.expect(rf"\[{tag}\] setOwnAddressType\(Public\) ok=[01]", timeout=5)
        dut.expect(rf"\[{tag}\] setOwnAddressType\(Random\) ok=[01]", timeout=5)
        dut.expect(rf"\[{tag}\] setOwnAddressType\(PublicID\) ok=[01]", timeout=5)
        dut.expect(rf"\[{tag}\] setOwnAddressType\(RandomID\) ok=[01]", timeout=5)

    client_success = 0
    for type_name in ("Public", "Random"):
        server.expect(rf"\[SERVER\] Phase17 set type={type_name} ok=[01]", timeout=10)
        server.expect(rf"\[SERVER\] Phase17 adv type={type_name} ok=[01]", timeout=10)

        m_scan = client.expect(
            rf"\[CLIENT\] Phase17 scan type={type_name} hit=([01]) advType=\d+",
            timeout=30,
        )
        hit = int(m_scan.group(1))
        if hit:
            m_conn = client.expect(
                rf"\[CLIENT\] Phase17 connect type={type_name} ok=([01])",
                timeout=20,
            )
            if int(m_conn.group(1)):
                client.expect(
                    rf"\[CLIENT\] Phase17 peer type={type_name} addr=\S+ connType=\d+",
                    timeout=10,
                )
                client_success += 1
        server.expect(
            rf"\[SERVER\] Phase17 type={type_name} connObserved=([01]) " rf"discObserved=[01] handle=\d+",
            timeout=25,
        )

    assert client_success == 2, f"expected both Public and Random connect roundtrips to succeed, got {client_success}"
    server.expect_exact("[SERVER] Phase17 done", timeout=15)
    client.expect_exact("[CLIENT] Phase17 done", timeout=15)


def _phase_authorization(server, client):
    """Phase 18 — application-level authorization callbacks approve/deny writes."""
    server.expect_exact("[SERVER] Phase18 authz ready", timeout=10)
    client.expect_exact("[CLIENT] Phase18 connected", timeout=30)
    client.expect(r"\[CLIENT\] Authz read: \S+", timeout=10)
    client.expect(r"\[CLIENT\] Authz write OK status: [01]", timeout=10)
    client.expect(r"\[CLIENT\] Authz write NO status: [01]", timeout=10)
    client.expect(r"\[CLIENT\] Authz readback: \S+", timeout=10)
    client.expect_exact("[CLIENT] Phase18 done", timeout=10)

    m = server.expect(
        r"\[SERVER\] Authz counters reads=(\d+) writesOK=(\d+) writesDeny=(\d+)\r?\n",
        timeout=30,
    )
    reads = int(m.group(1))
    writes_ok = int(m.group(2))
    writes_deny = int(m.group(3))
    assert reads >= 1, f"expected at least 1 authorized read, got {reads}"
    assert writes_ok >= 1, f"expected at least 1 authorized write, got {writes_ok}"
    assert writes_deny >= 1, f"expected at least 1 denied write, got {writes_deny}"
    server.expect(r"\[SERVER\] Authz lastAttrHandle=\d+", timeout=5)
    server.expect_exact("[SERVER] Phase18 done", timeout=10)


def _phase_encrypted_perm(server, client):
    """Phase 19 — encrypted characteristic reads/writes succeed over paired link."""
    client.expect_exact("[CLIENT] Phase19 connected", timeout=30)
    client.expect(r"\[CLIENT\] Enc read: \S+", timeout=10)
    client.expect_exact("[CLIENT] Enc write ok=1", timeout=10)
    # Post-pairing positive control: the link must be encrypted, authenticated
    # and bonded on both sides. Anything less means encryption/bonding regressed.
    client.expect_exact("[CLIENT] Phase19 sec enc=1 auth=1 bond=1", timeout=10)
    client.expect_exact("[CLIENT] Phase19 done", timeout=10)

    server.expect_exact("[SERVER] Phase19 encSec enc=1 auth=1 bond=1", timeout=15)
    server.expect_exact("[SERVER] Phase19 done", timeout=10)


def _phase_adv_data_and_scan(server, client):
    """Phase 20 — full BLEAdvertisementData payload + scan tuning parsers."""
    server.expect(r"\[SERVER\] Phase20 advReady ok=[01] isAdv=[01]", timeout=30)
    client.expect(r"\[CLIENT\] Phase20 results=\d+ isScanning=[01]", timeout=30)
    # Issue #12801: the ADV_ scan response ends with an AD whose length byte claims
    # 20 octets when only 2 follow. Rejecting it must leave the fields before it
    # intact — a parser missing the bounds check reads it as a 19-octet
    # manufacturer field and overwrites the genuine manufacturer data with
    # out-of-bounds bytes. The server also updates the primary manufacturer
    # field while retaining the same scan response; the final value must be
    # 0x02E5 + "NEW" ("e5024e4557"), not the cached "ESP" primary.
    # The client prints this before prefInterval.
    m_mfg = client.expect(
        r"\[CLIENT\] Phase20 mfgCompany=0x([0-9A-Fa-f]{4}) mfgLen=\d+ mfgHex=([0-9a-f]*)\r?\n",
        timeout=30,
    )
    assert int(m_mfg.group(1), 16) == 0x02E5, f"manufacturer company id corrupted: got 0x{m_mfg.group(1)}"
    mfg_hex = m_mfg.group(2).decode()
    assert mfg_hex == "e5024e4557", f"fresh primary manufacturer data was lost or corrupted: got {mfg_hex}"
    # Slave Connection Interval Range AD (0x12): encoding guard on the server
    # (setPreferredParams / setMinPreferred / setMaxPreferred) plus the
    # over-the-air round trip parsed back by the client. The client emits the
    # prefInterval line *before* sawAdv, so match it first (a later expect would
    # consume past it and never find it).
    m_cpref = client.expect(
        r"\[CLIENT\] Phase20 prefInterval have=([01]) min=0x([0-9A-Fa-f]+) max=0x([0-9A-Fa-f]+)\r?\n",
        timeout=30,
    )
    assert int(m_cpref.group(1)) == 1, "client did not receive the Slave Connection Interval Range AD (0x12)"
    assert int(m_cpref.group(2), 16) == 0x0006, f"preferred min interval mismatch: got 0x{m_cpref.group(2)}"
    assert int(m_cpref.group(3), 16) == 0x0012, f"preferred max interval mismatch: got 0x{m_cpref.group(3)}"
    server.expect_exact("[SERVER] Phase20 primaryRefresh ok=1", timeout=20)
    m_refresh = client.expect(r"\[CLIENT\] Phase20 primaryRefresh initial=([01]) refreshed=([01])", timeout=10)
    assert int(m_refresh.group(1)) == 1, "client did not observe the initial primary advertisement"
    assert int(m_refresh.group(2)) == 1, "cached scan response suppressed the refreshed primary advertisement"
    m_scan_rsp = client.expect(r"\[CLIENT\] Phase20 scanRspRefresh initial=([01]) refreshed=([01])", timeout=10)
    assert int(m_scan_rsp.group(1)) == 1, "client did not observe the initial scan response"
    assert int(m_scan_rsp.group(2)) == 1, "cached scan response replaced a fresh scan response"
    m_dup_clear = client.expect(r"\[CLIENT\] Phase20 duplicateCacheClear redispatched=([01])", timeout=10)
    assert int(m_dup_clear.group(1)) == 1, "clearDuplicateCache did not make an identical report observable again"
    m = client.expect(r"\[CLIENT\] Phase20 sawAdv=([01])", timeout=30)
    assert int(m.group(1)) == 1, "client did not see the ADV_<name> payload"
    # Parser regression: after the ADV_ window the server advertises OSZ_<name>,
    # whose scan response is built entirely out of malformed AD structures —
    # 16/32/128-bit UUID lists that are each one octet group short, then a
    # zero-length terminator hiding a well-formed manufacturer AD. None of them
    # may reach the parsed fields. Only one halting probe can be proven per
    # window, because the client parses the advertisement and the scan response
    # as one concatenated buffer and whatever halts parsing hides the rest.
    server.expect(r"\[SERVER\] Phase20 malformedAdv ok=1 isAdv=[01]", timeout=50)
    m_osz = client.expect(
        r"\[CLIENT\] Phase20 terminatorHidMfg=(-1|[01]) malformedUuidsRejected=(-1|[01])",
        timeout=50,
    )
    assert int(m_osz.group(1)) == 1, "client parsed a manufacturer AD hidden behind a zero-length AD terminator"
    assert int(m_osz.group(2)) == 1, "client parsed a UUID out of a malformed or terminated AD structure"
    m_manual = server.expect(r"\[SERVER\] Phase20 manualAdvCompletions=(\d+)", timeout=25)
    assert int(m_manual.group(1)) == 0, "manual advertising stop/restart incorrectly dispatched onComplete"
    m_timed = server.expect(
        r"\[SERVER\] Phase20 timedAdv start=([01]) completions=(\d+) isAdv=([01]) elapsed=(\d+)",
        timeout=10,
    )
    assert int(m_timed.group(1)) == 1, "timed advertising failed to start"
    assert int(m_timed.group(2)) == 1, "timed advertising did not dispatch onComplete exactly once"
    assert int(m_timed.group(3)) == 0, "timed advertising remained active after duration expiry"
    elapsed_ms = int(m_timed.group(4))
    assert 200 <= elapsed_ms <= 2500, f"300 ms advertising duration completed after {elapsed_ms} ms"
    m_cancel = server.expect(
        r"\[SERVER\] Phase20 cancelledAdv timedStart=([01]) stop=([01]) replacement=([01]) "
        r"completions=(\d+) isAdv=([01])",
        timeout=10,
    )
    assert all(
        int(m_cancel.group(i)) == 1 for i in range(1, 4)
    ), "timed advertising cancellation/restart operation failed"
    assert int(m_cancel.group(4)) == 0, "canceled advertising duration dispatched onComplete"
    assert int(m_cancel.group(5)) == 1, "a canceled duration stopped the replacement advertising session"
    server.expect_exact("[SERVER] Phase20 boundaryRestarts=12/12", timeout=30)
    # AD payload-limit regression: legacy 31-octet cap must be enforced (name
    # degrades to a Shortened Local Name, an oversized field is dropped whole),
    # while an extended payload accepts the full name.
    m_cap = server.expect(
        r"\[SERVER\] Phase20 adCap nameFit=([01]) oversizeRejected=([01]) extFullName=([01])",
        timeout=25,
    )
    assert int(m_cap.group(1)) == 1, "legacy AD overflowed the 31-octet cap on a too-long name"
    assert int(m_cap.group(2)) == 1, "an oversized AD field was not rejected whole (legacy cap)"
    assert int(m_cap.group(3)) == 1, "extended AD payload did not accept the full name"
    m_pref = server.expect(r"\[SERVER\] Phase20 prefIntervalAD ok=([01])", timeout=10)
    assert (
        int(m_pref.group(1)) == 1
    ), "setPreferredParams did not emit a correct Slave Connection Interval Range AD (0x12)"
    m_reset = server.expect(r"\[SERVER\] Phase20 resetWhileLive idle=([01]) restart=([01])", timeout=15)
    assert int(m_reset.group(1)) == 1, "reset() while advertising did not leave isAdvertising false"
    assert int(m_reset.group(2)) == 1, "start() after reset() while advertising failed"
    m_scan_off = server.expect(r"\[SERVER\] Phase20 scanRespOff ok=([01]) isAdv=([01])", timeout=15)
    assert (
        int(m_scan_off.group(1)) == 1 and int(m_scan_off.group(2)) == 1
    ), "start() with setScanResponse(false) failed (null/zero scan-rsp raw config)"
    m_dur = server.expect(
        r"\[SERVER\] Phase20 durationInvalid wrap=([01]) scanWrap=([01]) highDuty=([01]) noPeer=([01])",
        timeout=10,
    )
    assert (
        int(m_dur.group(1)) == 1 and int(m_dur.group(2)) == 1 and int(m_dur.group(3)) == 1 and int(m_dur.group(4)) == 1
    ), "duration overflow / high-duty directed forever / directed without peer must be InvalidParam"
    m_stop = server.expect(
        r"\[SERVER\] Phase20 failedStop fail=([01]) stuck=([01]) retry=([01]) idle=([01])",
        timeout=15,
    )
    assert (
        int(m_stop.group(1)) == 1
        and int(m_stop.group(2)) == 1
        and int(m_stop.group(3)) == 1
        and int(m_stop.group(4)) == 1
    ), "failed native advertising stop must leave isAdvertising set so a later stop() retries"
    m_restart = server.expect(r"\[SERVER\] Phase20 onCompleteRestart ok=([01])", timeout=15)
    assert int(m_restart.group(1)) == 1, "onComplete handler must be able to call start() without deadlocking"
    m_rebuild = server.expect(r"\[SERVER\] Phase20 rebuildFail rm=([01]) retry=([01]) skip=([01])", timeout=15)
    if int(m_rebuild.group(3)) == 0:
        assert (
            int(m_rebuild.group(1)) == 1 and int(m_rebuild.group(2)) == 1
        ), "failed NimBLE GATT rebuild must clear started so start() retries"
    m_legacy_stale = server.expect(
        r"\[SERVER\] Phase20 staleLegacyStop start=([01]) stillAdv=([01]) stop=([01]) idle=([01]) skip=([01])",
        timeout=15,
    )
    if int(m_legacy_stale.group(5)) == 0:
        assert (
            int(m_legacy_stale.group(1)) == 1
            and int(m_legacy_stale.group(2)) == 1
            and int(m_legacy_stale.group(3)) == 1
            and int(m_legacy_stale.group(4)) == 1
        ), (
            "leftover ADV_STOP_COMPLETE after start() must not clear isAdvertising "
            "(stop() would no-op while the radio is up)"
        )
    client.expect_exact("[CLIENT] Phase20 done", timeout=15)
    server.expect_exact("[SERVER] Phase20 done", timeout=20)


def _phase_beacon_and_eddystone(server, client):
    """Phase 21 — iBeacon / Eddystone URL / Eddystone TLM frame round-trip."""
    server.expect(r"\[SERVER\] Phase21 iBeacon adv ok=[01]", timeout=30)
    server.expect(r"\[SERVER\] Phase21 EddystoneURL ok=[01]", timeout=30)
    server.expect(r"\[SERVER\] Phase21 EddystoneEncryptedTLM ok=[01]", timeout=30)
    server.expect(r"\[SERVER\] Phase21 EddystoneUnknown01 ok=[01]", timeout=30)
    server.expect(r"\[SERVER\] Phase21 EddystoneTLM ok=[01]", timeout=30)

    m = client.expect(
        r"\[CLIENT\] Phase21 sawIBeacon=([01]) sawEddyURL=([01]) sawEddyTLM=([01]) "
        r"rejectedEncryptedTLM=([01]) eddy01Frame=(-?\d+)",
        timeout=50,
    )
    ib = int(m.group(1))
    ur = int(m.group(2))
    tl = int(m.group(3))
    assert ib == 1 and ur == 1 and tl == 1, f"expected all 3 beacon frames, got iBeacon={ib} url={ur} tlm={tl}"
    assert int(m.group(4)) == 1, "encrypted eTLM was decoded as plaintext telemetry"
    assert int(m.group(5)) == 0, "FEAA service-data starting with 0x01 must not classify as Eddystone TLM"
    server.expect_exact("[SERVER] Phase21 done", timeout=10)
    client.expect_exact("[CLIENT] Phase21 done", timeout=10)


def _phase_bond_and_whitelist(server, client):
    """Phase 22 — bond enumeration + whitelist + IRK retrieval + IRK cross-check.

    Both sides export their local IRK (distributed during SMP pairing) and the
    peer's IRK (received from the other side). A correct SMP exchange must
    satisfy local(server) == peer(client) AND local(client) == peer(server),
    which confirms the identity-resolving keys round-tripped the air.
    """
    irks = {}  # {(side, kind): hex_str}
    for tag, dut in [("SERVER", server), ("CLIENT", client)]:
        dut.expect(rf"\[{tag}\] Bond count: \d+", timeout=15)
        dut.expect(rf"\[{tag}\] Whitelist add=[01] isOn=[01] remove=[01] after=[01]", timeout=10)
        m_local = dut.expect(rf"\[{tag}\] LocalIRK present=([01]) hex=([0-9a-fA-F]*)\r?\n", timeout=10)
        m_peer = dut.expect(rf"\[{tag}\] PeerIRK present=([01]) hex=([0-9a-fA-F]*)\r?\n", timeout=10)
        irks[(tag, "local")] = (int(m_local.group(1)), m_local.group(2).decode().lower())
        irks[(tag, "peer")] = (int(m_peer.group(1)), m_peer.group(2).decode().lower())
    server.expect_exact("[SERVER] Phase22 done", timeout=10)
    client.expect_exact("[CLIENT] Phase22 done", timeout=10)

    # Cross-check: IRK distribution must be symmetric across the pair.
    for (tag, kind), (present, hexstr) in irks.items():
        assert present == 1, f"{tag} {kind} IRK missing — key distribution not negotiated"
        assert len(hexstr) == 32, f"{tag} {kind} IRK malformed: {hexstr}"
    assert (
        irks[("SERVER", "local")][1] == irks[("CLIENT", "peer")][1]
    ), f"server local IRK {irks[('SERVER', 'local')][1]} != client peer IRK {irks[('CLIENT', 'peer')][1]}"
    assert (
        irks[("CLIENT", "local")][1] == irks[("SERVER", "peer")][1]
    ), f"client local IRK {irks[('CLIENT', 'local')][1]} != server peer IRK {irks[('SERVER', 'peer')][1]}"


def _phase_error_paths_and_misc(server, client):
    """Phase 23 — unknown UUIDs, typed reads, UUID algebra, implicit str UUID conv."""
    # Server-side local API tests (run immediately, before client connects).
    server.expect(r"\[SERVER\] Services: \d+", timeout=15)
    server.expect(r"\[SERVER\] GetService known ok=[01] started=[01]", timeout=5)
    m_unk = server.expect(r"\[SERVER\] GetService unknown ok=([01])", timeout=5)
    assert int(m_unk.group(1)) == 0, "unknown service must NOT resolve"
    server.expect(r"\[SERVER\] Characteristics: \d+", timeout=5)
    server.expect(r"\[SERVER\] GetCharacteristic known ok=[01]", timeout=5)
    m_unkc = server.expect(r"\[SERVER\] GetCharacteristic unknown ok=([01])", timeout=5)
    assert int(m_unkc.group(1)) == 0, "unknown characteristic must NOT resolve"
    server.expect(r"\[SERVER\] Characteristic handle=\d+", timeout=5)
    server.expect(r"\[SERVER\] UUID bitSize16=16 bitSize128=128 to16=0x180D eq=\d+", timeout=5)
    server.expect(r"\[SERVER\] UUID bitSize32=32 to32=0x[0-9A-F]+", timeout=5)
    m_str = server.expect(r"\[SERVER\] StrConv pass=(\d+) fail=(\d+)\r?\n", timeout=5)
    assert int(m_str.group(1)) > 0, "server StrConv: expected at least one passing check"
    assert int(m_str.group(2)) == 0, "server StrConv: implicit const char* UUID conversion failed"

    # Client connects while server waits (server polls for up to 20 s).
    client.expect_exact("[CLIENT] Phase23 connected", timeout=30)
    m_us = client.expect(r"\[CLIENT\] Phase23 getService unknown ok=([01])", timeout=10)
    assert int(m_us.group(1)) == 0, "client: unknown service must NOT resolve"
    m_uc = client.expect(r"\[CLIENT\] Phase23 getChar unknown ok=([01])", timeout=10)
    assert int(m_uc.group(1)) == 0, "client: unknown char must NOT resolve"
    m_sv = client.expect(r"\[CLIENT\] Phase23 setValue clientOk=([01]) svcOk=([01])", timeout=10)
    assert int(m_sv.group(1)) == 1, "BLEClient::setValue round-trip failed"
    assert int(m_sv.group(2)) == 1, "BLERemoteService::setValue round-trip failed"
    client.expect(r"\[CLIENT\] Phase23 typed u8=\d+ u16=\d+ u32=\d+ f32=-?\d+", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 readBuf len=\d+", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 readRaw len=\d+ nonNull=[01]", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 UUID eq=1 ne=1 lt=1", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 UUID bitSize16=16 128=128 toUint16=0x180D", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 UUID valid=1 defaultBool=0", timeout=10)
    m_cache = client.expect(r"\[CLIENT\] Phase23 remote cache race clean=([01]) iterations=(\d+)", timeout=20)
    assert int(m_cache.group(1)) == 1 and int(m_cache.group(2)) > 0, "remote cache/parent getters raced tree teardown"
    client.expect(r"\[CLIENT\] Phase23 postDisc isConnected=[01]", timeout=15)
    m_wa = client.expect(r"\[CLIENT\] Phase23 writeAfterDisc ok=([01])", timeout=10)
    assert int(m_wa.group(1)) == 0, "write-after-disconnect must fail"
    m_discovery = client.expect(
        r"\[CLIENT\] Phase23 lazy discovery race clean=([01]) serviceResults=(\d+) descriptorResults=(\d+)",
        timeout=40,
    )
    assert int(m_discovery.group(1)) == 1, "lazy aggregate/targeted discovery raced cache invalidation"
    m_gen = client.expect(
        r"\[CLIENT\] Phase23 reconnect generation isolated=([01]) stale=InvalidState fresh=([01])",
        timeout=25,
    )
    assert (
        int(m_gen.group(1)) == 1 and int(m_gen.group(2)) == 1
    ), "a retained remote handle crossed a disconnect/reconnect generation"
    client.expect(r"\[CLIENT\] Phase23 connectAsync ok=[01]", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 cancelConnect ok=[01]", timeout=10)
    m_cstr = client.expect(r"\[CLIENT\] StrConv pass=(\d+) fail=(\d+)\r?\n", timeout=10)
    assert int(m_cstr.group(1)) > 0, "client StrConv: expected at least one passing check"
    assert int(m_cstr.group(2)) == 0, "client StrConv: implicit const char* UUID conversion failed"
    client.expect_exact("[CLIENT] Phase23 reAdvRace idle", timeout=10)

    # Targeted sends honor the client's CCCD (GATT §3.3.3.3): the client is
    # connected but not subscribed, and a handle with no link has no client.
    server.expect_exact("[SERVER] NotifyHandle ok=0 ret=InvalidState status=ERROR_NOTIFY_DISABLED", timeout=25)
    server.expect_exact("[SERVER] IndicateHandle ok=0 ret=InvalidState status=ERROR_INDICATE_DISABLED", timeout=5)
    server.expect_exact("[SERVER] NotifyBadHandle ok=0 ret=NotConnected status=ERROR_NO_CLIENT", timeout=5)
    server.expect_exact("[SERVER] IsSubscribed handle", timeout=5)
    m_adv_stop = server.expect(
        r"\[SERVER\] Phase23 advPreemptStop supported=([01]) stopped=([01])",
        timeout=15,
    )
    assert int(m_adv_stop.group(2)) == 1, "stop() during advertising preemption-resume did not cancel the resume"
    m_adv_rem = server.expect(
        r"\[SERVER\] Phase23 advRemaining supported=([01]) resumeMs=(\d+)",
        timeout=15,
    )
    if int(m_adv_rem.group(1)) == 1:
        resume_ms = int(m_adv_rem.group(2))
        assert 1500 <= resume_ms <= 2500, f"preempted 3-second advertising restarted its full duration ({resume_ms} ms)"
    else:
        assert int(m_adv_rem.group(2)) == 0
    server.expect_exact("[SERVER] Phase23 reAdvRaceReady", timeout=15)
    client.write("PHASE23_READV_GO\n")
    m_race_conn = client.expect(r"\[CLIENT\] Phase23 reAdvRace connected=([01])", timeout=20)
    server.expect_exact("[SERVER] Phase23 reAdvRaceHolding", timeout=15)
    client.write("PHASE23_READV_DISC\n")
    client.expect_exact("[CLIENT] Phase23 reAdvRace disconnected", timeout=15)
    m_race = server.expect(
        r"\[SERVER\] Phase23 advReAdvRace supported=([01]) hadPeer=([01]) resumeEntered=([01]) "
        r"queued=([01]) restarted=([01])",
        timeout=20,
    )
    assert int(m_race.group(5)) == 1, "advertiseOnDisconnect restart was dropped by a preemption resume"
    if int(m_race.group(1)) == 1:
        assert int(m_race_conn.group(1)) == 1, "reAdvRace client failed to connect"
        assert int(m_race.group(2)) == 1, "server did not see the reAdvRace peer"
        assert int(m_race.group(3)) == 1, "preemption resume did not enter"
        assert int(m_race.group(4)) == 1, "advertiseOnDisconnect restart was not queued"
    client.expect_exact("[CLIENT] Phase23 done", timeout=15)
    server.expect_exact("[SERVER] Phase23 done", timeout=25)


def _phase_ble5_advanced(server, client):
    """Phase 24 — BLE5 default PHY, coded PHY scan, per-connection PHY + DLE.

    Self-skips when either side prints '... not supported, skipping'.
    Returns True if the phase executed meaningfully on both sides.
    """
    m_s = server.expect(
        r"\[SERVER\] (Phase24 BLE5 not supported, skipping|Phase24 getDefaultPhy ok=[01] tx=\d+ rx=\d+)",
        timeout=20,
    )
    server_ok = b"getDefaultPhy" in m_s.group(0)

    # Read the client's capability before asserting the server's per-connection
    # results: the dedicated BLE5 connection those depend on only exists when
    # both sides speak BLE5. The two DUTs buffer independently, so reading the
    # client's line first here does not depend on the order they actually print.
    m_c = client.expect(
        r"\[CLIENT\] (Phase24 BLE5 not supported, skipping|Phase24 getDefaultPhy ok=[01] tx=\d+ rx=\d+)",
        timeout=20,
    )
    client_ok = b"getDefaultPhy" in m_c.group(0)
    both = server_ok and client_ok

    if server_ok:
        server.expect(r"\[SERVER\] Phase24 setDefaultPhy ok=1", timeout=5)
        if both:
            # The client holds a dedicated connection for this phase, so the
            # per-connection PHY/DLE path must run and succeed every run.
            server.expect_exact("[SERVER] Phase24 setPhy(2M) ok=1", timeout=15)
            server.expect(r"\[SERVER\] Phase24 getPhy ok=1 tx=\d+ rx=\d+", timeout=5)
            server.expect_exact("[SERVER] Phase24 setDataLen ok=1", timeout=5)
    server.expect_exact("[SERVER] Phase24 done", timeout=15)

    if client_ok:
        client.expect(r"\[CLIENT\] Phase24 setDefaultPhy ok=1", timeout=5)
        client.expect(r"\[CLIENT\] Phase24 startExtendedCoded ok=1", timeout=10)
        if both:
            client.expect_exact("[CLIENT] Phase24 phyConn ok=1", timeout=20)
    client.expect_exact("[CLIENT] Phase24 done", timeout=15)

    return both


def _phase_hid_smoke(server, client):
    """Phase 25 — HID-over-GATT spec compliance.

    Verifies all HoGP/HIDS additions:
      * Multiple Report characteristics (input + output) with distinct handles.
      * Boot Keyboard Input/Output caching (same handle on repeated calls).
      * Battery Service discoverable alongside HID Service.
      * Boot Input: Read + Notify properties.
      * Boot Output: Read + Write + WriteNR properties.
      * External Report Reference descriptor on Report Map.
    """
    server.expect_exact("[SERVER] Phase25 teardown shells=1", timeout=30)
    server.expect_exact("[SERVER] Phase25 services after begin: 0", timeout=30)
    server.expect_exact("[SERVER] Phase25 removal shells=1 detachedFactoriesRejected=1", timeout=10)
    server.expect_exact("[SERVER] Phase25 preRegRejected=4/4", timeout=10)
    server.expect(r"\[SERVER\] Phase25 hidChars=\d+", timeout=10)

    m_mr = server.expect(r"\[SERVER\] Phase25 multiReport=([01])", timeout=10)
    assert int(m_mr.group(1)) == 1, "inputReport + outputReport must produce distinct characteristics"

    m_cache = server.expect(r"\[SERVER\] Phase25 bootCached=([01])", timeout=10)
    assert int(m_cache.group(1)) == 1, "bootInput() must return the same handle on repeated calls"

    server.expect(r"\[SERVER\] Phase25 bootOut=[01]", timeout=10)
    m_dyn = server.expect(r"\[SERVER\] Phase25 dynSvcStarted=([01]) handle=(\d+)\r?\n", timeout=10)
    assert int(m_dyn.group(1)) == 1, "dynamic service addition after server.start() failed"
    server.expect(r"\[SERVER\] Phase25 HID adv ok=[01]", timeout=10)
    server.expect_exact("[SERVER] Phase25 HID ready", timeout=10)

    client.expect_exact("[CLIENT] Phase25 HID found", timeout=30)

    m_svcs = client.expect(
        r"\[CLIENT\] Phase25 services=(\d+) haveHid=([01]) haveBat=([01])",
        timeout=20,
    )
    assert int(m_svcs.group(2)) == 1, "HID Service (0x1812) must be discoverable"
    assert int(m_svcs.group(3)) == 1, "Battery Service (0x180F) must be discoverable"

    m_ch = client.expect(r"\[CLIENT\] Phase25 hidChars=(\d+)\r?\n", timeout=10)
    assert int(m_ch.group(1)) >= 5, (
        "HID service must expose at least 5 characteristics "
        "(report map, protocol mode, hid info, input report, output report)"
    )

    # These lines come from a single loop over characteristics, so their
    # order follows the GATT table: Report Map (extRefDesc) is registered
    # before Boot Input / Boot Output.
    m_ext = client.expect(r"\[CLIENT\] Phase25 extRefDesc=([01])", timeout=10)
    assert int(m_ext.group(1)) == 1, "Report Map must have an External Report Reference descriptor (0x2907)"

    client.expect(r"\[CLIENT\] Phase25 bootIn canRead=[01] canNotify=[01]", timeout=10)
    client.expect(r"\[CLIENT\] Phase25 bootOut canRead=[01] canWrite=[01] canWriteNR=[01]", timeout=10)

    m_rpt = client.expect(r"\[CLIENT\] Phase25 reportCount=(\d+)\r?\n", timeout=10)
    assert int(m_rpt.group(1)) >= 2, "Must have at least 2 Report characteristics (input + output)"

    m_boot = client.expect(
        r"\[CLIENT\] Phase25 bootInOk=([01]) bootOutOk=([01])",
        timeout=10,
    )
    assert int(m_boot.group(1)) == 1, "Boot Keyboard Input must have Read + Notify properties"
    assert int(m_boot.group(2)) == 1, "Boot Keyboard Output must have Read + Write + WriteNR properties"

    client.expect_exact("[CLIENT] Phase25 dynSvcRead=dyn_svc_ok", timeout=10)
    client.expect_exact("[CLIENT] Phase25 done", timeout=30)
    server.expect_exact("[SERVER] Phase25 done", timeout=30)


def _phase_ble5_legacy_over_ext(server, client):
    """Phase 26 — the public legacy advertising API stays discoverable and is
    surfaced as a *legacy*, connectable advertisement on a BLE5 ext-adv
    controller, where it is routed over the extended engine (DECISIONS.md D26).

    Returns True if the phase ran, False if BLE5 is unavailable (skip).
    """
    m_s = server.expect(
        r"\[SERVER\] (Phase26 BLE5 not supported, skipping|Phase26 legacyAdv ok=([01]) isAdv=([01]))",
        timeout=30,
    )
    if b"not supported" in m_s.group(0):
        server.expect_exact("[SERVER] Phase26 done", timeout=10)
        client.expect_exact("[CLIENT] Phase26 done", timeout=30)
        return False
    assert int(m_s.group(2)) == 1, "server legacy-over-ext advertising failed to start"

    m_c = client.expect(
        r"\[CLIENT\] (Phase26 BLE5 not supported, skipping|Phase26 sawLegacy=([01]) legacy=([01]) connectable=([01]))",
        timeout=30,
    )
    if b"not supported" in m_c.group(0):
        # Scanner cannot see an extended-engine advertisement, so there is
        # nothing to assert against even though the server advertised fine.
        server.expect_exact("[SERVER] Phase26 done", timeout=20)
        client.expect_exact("[CLIENT] Phase26 done", timeout=10)
        return False
    assert int(m_c.group(2)) == 1, "client did not discover the legacy-over-ext advertisement"
    assert int(m_c.group(3)) == 1, "legacy-over-ext adv was not flagged as a legacy advertisement"
    assert int(m_c.group(4)) == 1, "legacy-over-ext adv was not flagged connectable"
    server.expect_exact("[SERVER] Phase26 done", timeout=20)
    client.expect_exact("[CLIENT] Phase26 done", timeout=10)
    return True


def _phase_ble5_legacy_plus_ext(server, client):
    """Phase 27 — a legacy set (reserved top instance) and a user extended set
    (instance 0) advertise concurrently. The server must start BOTH, and the
    client must observe BOTH the legacy name (LEG_<name>, carried in the legacy
    scan response) and the extended-set name (EXT_<name>, only present in the
    extended PDU) — proving the reserved-instance concurrency of the unified
    engine (DECISIONS.md D26).

    Both sets share the peripheral's identity address, so on the scanner they
    collapse into a single address-keyed result whose per-report flags reflect
    whichever report arrived last; the LEG_/EXT_ names are therefore the
    reliable discriminator here. PDU-type flags are covered separately by
    phase 26 (legacy) and phase 2 (extended).

    Returns True if the phase ran, False if BLE5 is unavailable (skip).
    """
    m_s = server.expect(
        r"\[SERVER\] (Phase27 BLE5 not supported, skipping|Phase27 legacyAdv ok=([01]))",
        timeout=30,
    )
    if b"not supported" in m_s.group(0):
        server.expect_exact("[SERVER] Phase27 done", timeout=10)
        client.expect_exact("[CLIENT] Phase27 done", timeout=30)
        return False
    assert int(m_s.group(2)) == 1, "server legacy set failed to start"
    m_ext = server.expect(r"\[SERVER\] Phase27 extAdv ok=([01]) data=([01])", timeout=15)
    assert int(m_ext.group(2)) == 1, "server failed to set extended adv data"
    assert (
        int(m_ext.group(1)) == 1
    ), "server failed to start the concurrent extended set (reserved-instance concurrency broken)"

    m_c = client.expect(
        r"\[CLIENT\] (Phase27 BLE5 not supported, skipping"
        r"|Phase27 sawLegacy=([01]) sawExt=([01]) legacyIsLegacy=([01]) extIsLegacy=([01]))",
        timeout=30,
    )
    if b"not supported" in m_c.group(0):
        # The EXT_ set is only visible to an extended scanner, so a legacy-only
        # client cannot confirm the concurrency the server just demonstrated.
        server.expect_exact("[SERVER] Phase27 done", timeout=20)
        client.expect_exact("[CLIENT] Phase27 done", timeout=10)
        return False
    assert int(m_c.group(2)) == 1, "client did not see the concurrent legacy set (LEG_ name)"
    assert int(m_c.group(3)) == 1, "client did not see the concurrent extended set (EXT_ name)"
    server.expect_exact("[SERVER] Phase27 done", timeout=20)
    client.expect_exact("[CLIENT] Phase27 done", timeout=10)
    return True


def _phase_l2cap_bulk(server, client):
    """Phase 28 — L2CAP CoC bulk transfer across the credit-stall boundary +
    channel reconnect. Self-skips when L2CAP is unavailable.

    Returns True if the phase ran, False if skipped.
    """
    m_s = server.expect(
        r"\[SERVER\] Phase28 (L2CAP init OK|L2CAP not supported, skipping)",
        timeout=40,
    )
    if b"not supported" in m_s.group(0):
        server.expect_exact("[SERVER] Phase28 done", timeout=15)
        client.expect_exact("[CLIENT] Phase28 done", timeout=40)
        return False
    server.expect_exact("[SERVER] Phase28 oversized MTU rejected=1 heap=1", timeout=15)
    server.expect_exact("[SERVER] Phase28 duplicate PSM rejected=1 heap=1", timeout=15)

    m_c = client.expect(
        r"\[CLIENT\] Phase28 (L2CAP not supported, skipping|rescan found=[01])",
        timeout=40,
    )
    if b"not supported" in m_c.group(0):
        client.expect_exact("[CLIENT] Phase28 done", timeout=15)
        server.expect_exact("[SERVER] Phase28 done", timeout=40)
        return False

    server.expect_exact("[SERVER] Phase28 dropped server handle", timeout=15)
    client.expect_exact("[CLIENT] Phase28 stall armed write=1", timeout=15)
    server.expect_exact("[SERVER] Phase28 delayed L2CAP accept entered", timeout=15)
    client.write("PHASE28_STALL_GO\n")
    m_timeout = client.expect(r"\[CLIENT\] Phase28 timeout returned empty=([01]) elapsed=(\d+)", timeout=10)
    assert int(m_timeout.group(1)) == 1, "connectL2CAP did not return an empty handle after its timeout"
    server.expect_exact("[SERVER] Phase28 delayed L2CAP accept released", timeout=10)
    server.expect_exact("[SERVER] Phase28 L2CAP channel accepted count=1", timeout=10)
    server.expect_exact("[SERVER] Phase28 L2CAP channel disconnected count=1", timeout=10)
    client.write("PHASE28_CLEANUP_GO\n")
    m_abandoned = client.expect(r"\[CLIENT\] Phase28 abandoned clean=([01]) elapsed=(\d+) write=([01])", timeout=10)
    assert (
        int(m_abandoned.group(1)) == 1
    ), "late L2CAP connect cleanup corrupted state or returned outside the timeout window"
    assert int(m_abandoned.group(3)) == 1, "failed to arm the delayed-peer L2CAP timeout scenario"
    client.expect_exact("[CLIENT] Phase28 L2CAP channel connected", timeout=30)
    # Bulk SDU must arrive intact (server echoes the accumulated byte count once
    # it reaches the target, proving credit replenishment mid transfer worked).
    m_ack = client.expect(r"\[CLIENT\] Phase28 bulkAck ok=([01]) total=(\d+)", timeout=60)
    assert int(m_ack.group(1)) == 1, f"L2CAP bulk transfer size mismatch (server acked {m_ack.group(2).decode()} bytes)"
    server.expect(r"\[SERVER\] Phase28 bulk got=\d+", timeout=30)
    m_peer = server.expect(r"\[SERVER\] Phase28 peer race disconnect=(-?\d+)", timeout=30)
    assert int(m_peer.group(1)) in (0, -12), "host-callback peer disconnect returned an unexpected status"
    m_race = client.expect(
        r"\[CLIENT\] Phase28 race clean=([01]) write=(-?\d+) mtu=(\d+) disconnect=(-?\d+)",
        timeout=40,
    )
    assert int(m_race.group(1)) == 1, "L2CAP peer-disconnect race returned a dirty status or corrupted the heap"
    m_churn = client.expect(r"\[CLIENT\] Phase28 event churn clean=([01]) count=(\d+)", timeout=90)
    assert (
        int(m_churn.group(1)) == 1 and int(m_churn.group(2)) == 64
    ), "per-channel operation events exhausted or leaked the NPL pool"
    server.expect_exact("[SERVER] Phase28 shutdown stall entered", timeout=30)
    client.write("PHASE28_END_GO\n")
    m_end = client.expect(r"\[CLIENT\] Phase28 end in-flight=([01]) released=([01]) status=(-?\d+)", timeout=15)
    assert int(m_end.group(1)) == 1, "BLE.end() was not invoked while the L2CAP write was genuinely in flight"
    assert (
        int(m_end.group(2)) == 1 and int(m_end.group(3)) == -7
    ), "BLE.end() did not release the stalled writer with NotConnected"
    server.expect_exact("[SERVER] Phase28 shutdown stall released", timeout=10)
    m_drop = client.expect(r"\[CLIENT\] Phase28 final handle drop clean=([01])", timeout=30)
    assert (
        int(m_drop.group(1)) == 1
    ), "dropping the final detached handle raced its queued host callback or corrupted the heap"
    m_shell = client.expect(
        r"\[CLIENT\] Phase28 retained clean=([01]) end=(-?\d+)/(\d+)/(-?\d+) rebegin=([01]) "
        r"after=(-?\d+)/(\d+)/(-?\d+)",
        timeout=30,
    )
    assert int(m_shell.group(1)) == 1, "retained L2CAP shell or operation event was unsafe across end/rebegin"
    assert int(m_shell.group(5)) == 1, "BLE rebegin failed after queued L2CAP operation cleanup"
    assert (int(m_shell.group(2)), int(m_shell.group(3)), int(m_shell.group(4))) == (
        -7,
        0,
        -7,
    ), "detached shell status after end was not NotConnected"
    assert (int(m_shell.group(6)), int(m_shell.group(7)), int(m_shell.group(8))) == (
        -7,
        0,
        -7,
    ), "detached shell status after rebegin was not NotConnected"
    # Fresh channel after teardown must ping/pong — the accept path recovered.
    m_re = client.expect(r"\[CLIENT\] Phase28 reconnect ok=([01])", timeout=40)
    assert int(m_re.group(1)) == 1, "L2CAP channel reconnect ping/pong failed"
    m_ops = client.expect(r"\[CLIENT\] Phase28 operation race ok=([01]) lifecycle=([01])", timeout=10)
    assert int(m_ops.group(1)) == 1 and int(m_ops.group(2)) == 1, "L2CAP operation race/lifecycle regression failed"

    client.expect_exact("[CLIENT] Phase28 done", timeout=15)
    server.expect_exact("[SERVER] Phase28 done", timeout=130)
    return True


def _phase_conn_params_reject(server, client):
    """Phase 29 — the central rejects the peripheral's connection-parameter
    update from its pre-accept hook. NimBLE-only: Bluedroid negotiates conn
    params inside L2CAP with no pre-accept surface, so the client self-skips.

    Returns True if the phase ran (hook fired), False if skipped (Bluedroid).
    """
    # If the client never (re)discovered the server there is nothing to
    # exercise — treat it as a skip rather than a hard failure.
    m_f = client.expect(r"\[CLIENT\] Phase29 rescan found=([01]) nimble=([01])", timeout=45)
    is_nimble = int(m_f.group(2)) == 1
    if int(m_f.group(1)) == 0:
        client.expect_exact("[CLIENT] Phase29 done", timeout=15)
        server.expect(r"\[SERVER\] Phase29 (requestSent ok=[01]|no peer connected|rebuild FAILED)", timeout=30)
        server.expect_exact("[SERVER] Phase29 done", timeout=20)
        assert not is_nimble, "Phase29: NimBLE client failed to rescan the server (not a skip)"
        return False

    server.expect(r"\[SERVER\] Phase29 (requestSent ok=[01]|no peer connected)", timeout=30)
    m_c = client.expect(
        r"\[CLIENT\] Phase29 hookFired=([01]) beforeInterval=(\d+) afterInterval=(\d+) held=([01])",
        timeout=40,
    )
    client.expect_exact("[CLIENT] Phase29 done", timeout=10)
    server.expect_exact("[SERVER] Phase29 done", timeout=20)

    if int(m_c.group(1)) == 0:
        assert not is_nimble, "Phase29: conn-params pre-accept hook did not fire on NimBLE"
        # No pre-accept hook on this backend (Bluedroid) — nothing to assert.
        return False
    assert int(m_c.group(4)) == 1, (
        f"a rejected conn-param update still changed the live interval "
        f"({m_c.group(2).decode()} -> {m_c.group(3).decode()})"
    )
    return True


def _phase_deletebond_roundtrip(server, client):
    """Phase 30 — deleteBond fully round-trips: an existing bond is deleted on
    both ends, the store is confirmed empty, then reading the authenticated
    characteristic forces a fresh pairing that re-establishes the bond.
    """
    server.expect_exact("[SERVER] Services after begin: 0", timeout=45)
    server.expect(r"\[SERVER\] Phase30 bondsBefore=\d+", timeout=20)
    m_sdel = server.expect(r"\[SERVER\] Phase30 deleteOk=([01])", timeout=15)
    assert int(m_sdel.group(1)) == 1, "server deleteBond() failed"
    server.expect(r"\[SERVER\] Phase30 bondsAfterDelete=\d+", timeout=15)
    m_adv = server.expect(r"\[SERVER\] Phase30 isAdvAfterDelete=([01])", timeout=15)
    assert int(m_adv.group(1)) == 1, "advertising did not survive deleteBond() (stack preempt was not resumed)"

    m_cb = client.expect(r"\[CLIENT\] Phase30 bondsBefore=(\d+)\r?\n", timeout=45)
    m_scan = client.expect(r"\[CLIENT\] Phase30 preemptScan start=([01]) complete=([01]) elapsed=(\d+)", timeout=15)
    assert int(m_scan.group(1)) == 1, "finite scan did not start before deleteBond"
    assert int(m_scan.group(2)) == 1, "preempted finite scan did not complete"
    assert int(m_scan.group(3)) >= 3500, f"preempted 4-second scan completed prematurely ({int(m_scan.group(3))} ms)"
    assert (
        int(m_scan.group(3)) <= 5500
    ), f"preempted 4-second scan restarted its full duration ({int(m_scan.group(3))} ms)"
    m_replace = client.expect(
        r"\[CLIENT\] Phase30 sessionReplace first=([01]) replacement=([01]) completions=(\d+) "
        r"elapsed=(\d+) stopped=([01])",
        timeout=10,
    )
    assert int(m_replace.group(1)) == 1 and int(m_replace.group(2)) == 1, "scan session replacement failed to start"
    assert int(m_replace.group(3)) == 1, "superseded scan completion leaked into the replacement session"
    assert 500 <= int(m_replace.group(4)) <= 2000, "replacement scan completed at the wrong session deadline"
    assert int(m_replace.group(5)) == 1, "replacement scan remained active after completion"
    m_preempt_stop = client.expect(r"\[CLIENT\] Phase30 preemptStop clean=([01]) retained=([01])", timeout=10)
    assert (
        int(m_preempt_stop.group(1)) == 1
    ), "stop during native scan preemption leaked or freed its callback context early"
    m_churn = client.expect(r"\[CLIENT\] Phase30 scanChurn clean=([01]) heapDelta=(-?\d+)", timeout=15)
    assert (
        int(m_churn.group(1)) == 1
    ), f"repeated scan start/stop leaked or corrupted heap ({int(m_churn.group(2))} bytes)"
    m_host_churn = client.expect(r"\[CLIENT\] Phase30 hostScanChurn clean=([01]) cycles=(0|48)", timeout=15)
    assert int(m_host_churn.group(1)) == 1, "callback-context scan churn failed or exhausted NimBLE events"
    m_cdel = client.expect(r"\[CLIENT\] Phase30 deleteOk=([01])", timeout=15)
    assert int(m_cdel.group(1)) == 1, "client deleteBond() failed"
    m_cd = client.expect(r"\[CLIENT\] Phase30 bondsAfterDelete=(\d+)\r?\n", timeout=15)
    assert int(m_cb.group(1)) >= 1, "client had no bond to delete (phase 10 pairing missing?)"
    assert int(m_cd.group(1)) == 0, "client bond store not empty after deleteBond"

    m_cr = client.expect(
        r"\[CLIENT\] Phase30 reEnc=([01]) reBond=([01]) bondsAfterRepair=(\d+)\r?\n",
        timeout=45,
    )
    assert int(m_cr.group(1)) == 1, "link not encrypted after deleteBond re-pair"
    assert int(m_cr.group(2)) == 1, "link not bonded after deleteBond re-pair"
    assert int(m_cr.group(3)) >= 1, "bond store still empty after re-pair (deleteBond round-trip failed)"

    m_sr = server.expect(
        r"\[SERVER\] Phase30 rebonded=([01]) bondsAfterRepair=(\d+) identityCount=(\d+)\r?\n", timeout=45
    )
    assert int(m_sr.group(1)) == 1, "server did not observe a bonded re-pair"
    assert int(m_sr.group(3)) == 1, f"server onIdentity fired {int(m_sr.group(3))} times on re-pair, expected 1"

    client.expect_exact("[CLIENT] Phase30 done", timeout=15)
    server.expect_exact("[SERVER] Phase30 done", timeout=15)


def _phase_nc_reject(server, client):
    """Phase 31 — an explicit Numeric-Comparison reject must abort pairing: the
    client returns false from its confirm handler, so the authenticated read
    fails and neither end ends up encrypted or bonded. Runs on both stacks.
    """
    m_c = client.expect(
        r"\[CLIENT\] Phase31 ncReject readOk=([01]) enc=([01]) bond=([01]) bonds=(\d+) "
        r"authFired=([01]) authSuccess=([01]) identityCount=(\d+)",
        timeout=60,
    )
    assert int(m_c.group(1)) == 0, "secure read succeeded despite the numeric-comparison reject"
    assert int(m_c.group(2)) == 0, "link encrypted despite the numeric-comparison reject"
    assert int(m_c.group(3)) == 0, "link bonded despite the numeric-comparison reject"
    assert int(m_c.group(4)) == 0, "a bond persisted despite the numeric-comparison reject"
    assert int(m_c.group(5)) == 1, "auth-complete did not fire on the rejecting central"
    assert int(m_c.group(6)) == 0, "auth-complete reported success despite the reject"
    assert int(m_c.group(7)) == 0, f"client onIdentity fired {int(m_c.group(7))} times despite failed pairing"

    m_s = server.expect(r"\[SERVER\] Phase31 ncReject sawPeer=([01]) bonds=(\d+) identityCount=(\d+)\r?\n", timeout=60)
    assert int(m_s.group(2)) == 0, "server retained a bond despite the numeric-comparison reject"
    assert int(m_s.group(3)) == 0, f"server onIdentity fired {int(m_s.group(3))} times despite failed pairing"

    client.expect_exact("[CLIENT] Phase31 done", timeout=15)
    server.expect_exact("[SERVER] Phase31 done", timeout=15)


def _phase_passkey_entry(server, client):
    """Phase 32 — passkey-entry pairing: the peripheral is DisplayOnly with a
    fixed static passkey and the central is KeyboardOnly and supplies the same
    value, forcing the SMP Passkey Entry method. The authenticated read must
    succeed and both ends must end up encrypted + bonded.
    """
    m_c = client.expect(
        r"\[CLIENT\] Phase32 passkeyEntry enc=([01]) bond=([01]) bonds=(\d+) authFired=([01]) authSuccess=([01])",
        timeout=75,
    )
    assert int(m_c.group(1)) == 1, "passkey-entry pairing did not encrypt the link"
    assert int(m_c.group(2)) == 1, "passkey-entry pairing did not bond"
    assert int(m_c.group(3)) >= 1, "no bond stored after passkey-entry pairing"

    m_s = server.expect(r"\[SERVER\] Phase32 passkeyEntry rebonded=([01]) bonds=(\d+)\r?\n", timeout=75)
    assert int(m_s.group(1)) == 1, "server did not observe a bonded passkey-entry pairing"
    assert int(m_s.group(2)) >= 1, "server stored no bond after passkey-entry pairing"

    client.expect_exact("[CLIENT] Phase32 done", timeout=15)
    server.expect_exact("[SERVER] Phase32 done", timeout=15)


def _phase_indication_busy_retry(server, client):
    """Phase 33 — indications queued right behind one the stack sent itself.
    On Bluedroid, deleting a started service makes the stack indicate Service
    Changed; the server's next indications are refused with ESP_GATT_BUSY until
    the client confirms it. The library must retry them, so all three are
    confirmed. On NimBLE they simply go out (serviceChanged=n/a)."""
    m_re = client.expect(
        r"\[CLIENT\] Phase33 stack=(NimBLE|Bluedroid) staleRejected=([01]) rediscovered=([01]) resubscribed=([01]) "
        r"payload4=([01]) fullRange=([01])",
        timeout=60,
    )
    assert int(m_re.group(2)) == 1, "Phase33: backend retained a stale remote handle after Service Changed"
    assert (
        int(m_re.group(3)) == 1 and int(m_re.group(4)) == 1
    ), "Phase33: client did not rediscover and restore subscriptions after Service Changed"
    assert int(m_re.group(5)) == 1, "Phase33: Service Changed callback payload was not the required 4-byte handle range"
    if m_re.group(1) == b"Bluedroid":
        assert int(m_re.group(6)) == 1, "Phase33: Bluedroid did not report its documented conservative full range"
    server.write("PHASE33_CONTINUE\n")
    m = server.expect(
        r"\[SERVER\] Phase33 (no subscriber|serviceChanged=(0|1|n/a) confirmed=(\d+) failed=(\d+))\r?\n",
        timeout=60,
    )
    m_rx = client.expect(r"\[CLIENT\] Phase33 svcChangedRx=(\d+) svcSubOk=([01]) busyPayloads=(\S*)\r?\n", timeout=60)
    client.expect_exact("[CLIENT] Phase33 found=1 subscribed=1", timeout=60)
    assert m.group(1) != b"no subscriber", "Phase33: client never subscribed"
    service_changed = m.group(2).decode()
    confirmed = int(m.group(3))
    failed = int(m.group(4))
    svc_rx = int(m_rx.group(1))
    busy_payloads = m_rx.group(3).decode()
    LOGGER.info(
        f"Phase33: serviceChanged={service_changed} svcChangedRx={svc_rx} "
        f"confirmed={confirmed} failed={failed} busyPayloads={busy_payloads}"
    )
    # The server line only proves removeService() succeeded; the client line
    # proves a Service Changed indication was actually received over the air.
    assert int(m_rx.group(2)) == 1, "Phase33: client failed to subscribe to Service Changed (0x2A05)"
    assert svc_rx >= 1, "Phase33: client never received a Service Changed indication"
    assert (
        confirmed == 3 and failed == 0
    ), f"Phase33: expected 3 confirmed indications, got confirmed={confirmed} failed={failed}"
    assert (
        busy_payloads == "busy_1,busy_2,busy_3"
    ), f"Phase33: client received '{busy_payloads}', expected 'busy_1,busy_2,busy_3'"
    server.expect_exact("[SERVER] Phase33 done", timeout=20)
    client.expect_exact("[CLIENT] Phase33 done", timeout=20)


def _phase_periodic_adv_lifecycle(server, client):
    """Phase 34 — periodic-advertising teardown transitions the happy path never
    reached: an explicit terminatePeriodicSync and an onPeriodicLost after the
    server stops the train. Self-skips without BLE5.

    Returns True if the phase ran, False if skipped.
    """
    m_s = server.expect(
        r"\[SERVER\] Phase34 (periodic started|BLE5 not supported, skipping)",
        timeout=30,
    )
    if b"not supported" in m_s.group(0):
        server.expect_exact("[SERVER] Phase34 done", timeout=15)
        client.expect_exact("[CLIENT] Phase34 done", timeout=40)
        return False

    m_c = client.expect(
        r"\[CLIENT\] Phase34 (synced=([01]) dataRx=([01])|BLE5 not supported, skipping)",
        timeout=45,
    )
    if b"not supported" in m_c.group(0):
        client.expect_exact("[CLIENT] Phase34 done", timeout=15)
        server.expect_exact("[SERVER] Phase34 done", timeout=45)
        return False
    assert int(m_c.group(2)) == 1, "periodic sync not established"
    assert int(m_c.group(3)) == 1, "periodic report not received"

    # At least one teardown transition must be observed: an explicit terminate
    # (deterministic) or a supervision-timeout sync-lost (timing dependent).
    m_t = client.expect(r"\[CLIENT\] Phase34 lost=([01]) terminateOk=([01])", timeout=45)
    assert (int(m_t.group(1)) == 1) or (
        int(m_t.group(2)) == 1
    ), "neither sync-lost nor explicit terminate observed (periodic teardown uncovered)"

    server.expect_exact("[SERVER] Phase34 periodic stopped", timeout=45)
    client.expect_exact("[CLIENT] Phase34 done", timeout=20)
    server.expect_exact("[SERVER] Phase34 done", timeout=20)
    return True


def _phase_pending_send_edges(server, client):
    """Phase 35 — CCCD notify-vs-indicate bit routing on one ACL, in-flight
    removeService Busy, short-timeout read then destroy/re-read, and a notify
    flood. Per-peer CCCD, EATT, and withheld Handle Value Confirmation need a
    third DUT or a public EATT API / a non-confirming peer.
    """
    m_rb_ready = client.expect(r"\[CLIENT\] Phase35 rollbackReady fail=([01])", timeout=45)
    assert int(m_rb_ready.group(1)) == 1, "CCCD write failure must fail subscribe before the notify overlap window"
    server.write("PHASE35_CCCD_GO\n")
    m_sub_rb = client.expect(r"\[CLIENT\] Phase35 subscribeRollback fail=([01]) resub=([01])", timeout=45)
    assert (
        int(m_sub_rb.group(1)) == 1 and int(m_sub_rb.group(2)) == 1
    ), "CCCD write failure must roll back the notify callback and allow a later subscribe"
    m_cmd = client.expect(
        r"\[CLIENT\] Phase35 callback commands clean=([01]) overlap=([01]) state=([01]) "
        r"accepted=(\d+) busy=(\d+) other=(\d+)",
        timeout=30,
    )
    assert int(m_cmd.group(1)) == 1 and int(m_cmd.group(2)) == 1 and int(m_cmd.group(3)) == 1
    bluedroid_commands = int(m_cmd.group(5)) == 1
    assert int(m_cmd.group(4)) + int(m_cmd.group(5)) + int(m_cmd.group(6)) == (10 if bluedroid_commands else 9)
    m_cccd = server.expect(r"\[SERVER\] Phase35 autoCCCD supported=([01]) owner=([01])", timeout=30)
    assert int(m_cccd.group(1)) == int(bluedroid_commands) and int(m_cccd.group(2)) == 1
    m_cmd_rx = server.expect(r"\[SERVER\] Phase35 callback command rx=(\d+)", timeout=30)
    assert int(m_cmd_rx.group(1)) >= 8
    m_c = client.expect(
        r"\[CLIENT\] Phase35 found=1 connected=1 notify=(\d+) indicate=(\d+)\r?\n",
        timeout=45,
    )
    assert int(m_c.group(1)) >= 1, "notify-only CCCD received no notification"
    assert int(m_c.group(2)) >= 1, "indicate-only CCCD received no indication"

    m_s_peer = server.expect(
        r"\[SERVER\] Phase35 cccd notifyOk=(\d+) indicateDisabled=(\d+) indicateOk=(\d+) notifyDisabled=(\d+)\r?\n",
        timeout=45,
    )
    assert int(m_s_peer.group(1)) >= 1, "notify() did not complete while CCCD notify was enabled"
    assert int(m_s_peer.group(2)) >= 1, "indicate() while CCCD was notify-only was not ERROR_INDICATE_DISABLED"
    assert int(m_s_peer.group(3)) >= 1, "indicate() did not complete after CCCD switched to indicate"
    assert int(m_s_peer.group(4)) >= 1, "notify() while CCCD was indicate-only was not ERROR_NOTIFY_DISABLED"

    client.expect_exact("[CLIENT] Phase35 mixedCccd armed=1", timeout=15)
    m_mixed = server.expect(
        r"\[SERVER\] Phase35 mixedSend first=(\S+) second=(\S+) steal=([01])",
        timeout=20,
    )
    assert (
        m_mixed.group(1) == b"SUCCESS_INDICATE"
    ), f"indication confirmation was demuxed as {m_mixed.group(1).decode()} while a notify was only reserved"
    assert (
        m_mixed.group(2) == b"SUCCESS_NOTIFY"
    ), f"held notification completed as {m_mixed.group(2).decode()} after the indication"
    assert int(m_mixed.group(3)) == 0, "pending notify reservation stole the indication confirmation"

    m_mixed_sub = server.expect(
        r"\[SERVER\] Phase35 mixedSendSubmitted first=(\S+) second=(\S+) steal=([01])",
        timeout=20,
    )
    mixed_sub_statuses = {m_mixed_sub.group(1), m_mixed_sub.group(2)}
    assert mixed_sub_statuses == {b"SUCCESS_INDICATE", b"SUCCESS_NOTIFY"}, (
        "indicate() then notify() must complete as one SUCCESS_INDICATE and one SUCCESS_NOTIFY, "
        f"got first={m_mixed_sub.group(1).decode()} second={m_mixed_sub.group(2).decode()}"
    )

    m_cf = client.expect(r"\[CLIENT\] Phase35 flood=(\d+)\r?\n", timeout=20)
    assert int(m_cf.group(1)) >= 1, "client received no flooded notifications"
    m_qrx = client.expect(r"\[CLIENT\] Phase35 queueRx=(\d+)\r?\n", timeout=20)
    assert int(m_qrx.group(1)) >= 8, "client did not receive queued indications"
    server.expect_exact("[SERVER] Phase35 localReadRemove ready", timeout=20)
    client.write("PHASE35_READ_REMOVE_GO\n")
    client.expect_exact("[CLIENT] Phase35 localReadRemove armed=1", timeout=20)
    m_rm = server.expect(
        r"\[SERVER\] Phase35 removeBusy=([01]) accepted=(\d+) busy=(\d+) readEntered=([01]) "
        r"removeStarted=([01]) removeDone=([01]) removeIdle=([01])",
        timeout=20,
    )
    assert int(m_rm.group(1)) == 1, "removeService of a service with in-flight indications did not return Busy"
    assert int(m_rm.group(2)) >= 8, "in-flight removeService did not fill the indication queue"
    assert all(int(m_rm.group(i)) == 1 for i in range(4, 7)), "local GATT read/remove barrier did not overlap"
    remove_idle = int(m_rm.group(7))
    LOGGER.info("Phase35 removeIdle=%d", remove_idle)
    assert remove_idle == 1, "removeService of an idle service after queue drained failed"

    m_fl = server.expect(
        r"\[SERVER\] Phase35 flood sent=(\d+) confirmed=(\d+) failed=(\d+)\r?\n",
        timeout=25,
    )
    sent, confirmed, failed = (int(m_fl.group(i)) for i in range(1, 4))
    LOGGER.info(f"Phase35 flood sent={sent} confirmed={confirmed} failed={failed}")
    assert sent == 32, f"notify flood sent {sent} of 32"
    assert confirmed == sent and failed == 0, f"notify flood did not complete: confirmed={confirmed} failed={failed}"
    m_read_remove = client.expect(r"\[CLIENT\] Phase35 localReadRemove clean=([01]) attempts=(\d+)", timeout=20)
    assert int(m_read_remove.group(1)) == 1, "local GATT read/remove race did not finish cleanly"
    assert int(m_read_remove.group(2)) > 0, "local GATT read/remove race attempted no reads"

    m_to = client.expect(
        r"\[CLIENT\] Phase35 timeoutEmpty=([01]) overlapRejected=([01]) reread=([01]) "
        r"commandLateIsolated=([01]) writeLateIsolated=([01])",
        timeout=30,
    )
    assert int(m_to.group(1)) == 1, "short readValue timeout did not fail (handle destroy/re-read untested)"
    assert int(m_to.group(2)) == 1, "overlapping read reused a completion slot abandoned by timeout"
    assert int(m_to.group(3)) == 1, "read after timeout + destroy did not return the characteristic value"
    assert int(m_to.group(4)) == 1, "late write-command completion was attributed to a newer command"
    assert int(m_to.group(5)) == 1, "late confirmed-write completion was attributed to a newer write"
    client.expect_exact("[CLIENT] Phase35 notifyTeardown trigger=1", timeout=15)
    server.expect_exact("[SERVER] Phase35 notifyTeardown burst entered", timeout=15)
    client.write("PHASE35_DISCONNECT_GO\n")
    if bluedroid_commands:
        server.expect_exact("[SERVER] Phase35 owner drop received", timeout=15)
        client.expect_exact("[CLIENT] Phase35 owner handles dropped=1", timeout=15)
        server.write("PHASE35_OWNER_DROP_GO\n")
    client.expect_exact("[CLIENT] Phase35 notifyTeardown armed=1 pendingOwner=1 disconnected=1", timeout=15)
    m_re_adv = server.expect(
        r"\[SERVER\] Phase35 reAdvStop entered=([01]) stop=([01]) idle=([01])",
        timeout=20,
    )
    assert (
        int(m_re_adv.group(1)) == 1 and int(m_re_adv.group(2)) == 1 and int(m_re_adv.group(3)) == 1
    ), "in-flight advertiseOnDisconnect restart must be waited out by stop() so advertising is idle"
    m_td = server.expect(
        r"\[SERVER\] Phase35 notifyTeardown stack=(NimBLE|Bluedroid) attempted=(\d+) "
        r"accepted=(\d+) atDisconnect=(\d+) final=(\d+) success=(\d+) noClient=(\d+) "
        r"other=(\d+) late=(\d+)",
        timeout=20,
    )
    attempted, accepted, at_disconnect, final, success, no_client, other, late = (
        int(m_td.group(i)) for i in range(2, 10)
    )
    assert accepted > 0, "notification teardown scenario accepted no sends"
    assert (
        at_disconnect == attempted and final == attempted
    ), "notification attempts did not receive exactly one terminal status before onDisconnect"
    assert late == 0, "notification status arrived after onDisconnect"
    if m_td.group(1) == b"Bluedroid":
        assert (
            success + no_client == attempted and other == 0
        ), "Bluedroid notification attempt did not receive success or link-loss status"
        assert no_client > 0, "Bluedroid teardown left no pending notification to exercise ERROR_NO_CLIENT"
    else:
        assert (
            success + no_client == accepted
        ), "NimBLE accepted notification did not receive success or link-loss status"
        assert other == attempted - accepted, "NimBLE synchronous rejection did not receive exactly one error status"
    client.expect_exact(
        f"[CLIENT] Phase35 terminalShutdown supported={int(bluedroid_commands)} clean=1",
        timeout=30,
    )
    server.expect_exact("[SERVER] Phase35 BLEStream final release waiting", timeout=20)
    server.write("PHASE35_STREAM_GO\n")
    server.expect_exact("[SERVER] Phase35 BLEStream final release ready=1", timeout=20)
    client.expect_exact("[CLIENT] Phase35 BLEStream final release connected=1", timeout=60)
    server.write("PHASE35_STREAM_CONNECTED\n")
    server.expect_exact("[SERVER] Phase35 BLEStream final release connected=1", timeout=10)
    client.write("PHASE35_STREAM_RELEASE_GO\n")
    client.expect_exact("[CLIENT] Phase35 BLEStream final release clean=1", timeout=30)
    server.expect_exact("[SERVER] Phase35 BLEStream final release done=1", timeout=20)
    server.expect_exact("[SERVER] Phase35 done", timeout=20)
    client.expect_exact("[CLIENT] Phase35 done", timeout=20)


def _assert_oob_reject(match, label):
    found, connected, read_ok, enc, bond, auth_fired, auth_success = (int(match.group(i)) for i in range(1, 8))
    assert found == 1 and connected == 1, f"{label}: client did not find and connect to the server"
    assert (
        read_ok == 0 and enc == 0 and bond == 0
    ), f"{label}: OOB pairing must not encrypt, bond, or complete a secure read"
    assert (
        auth_fired == 1 and auth_success == 0
    ), f"{label}: library must report authentication failure, not hang or succeed"


def _phase_directed_adv_and_oob(server, client):
    m_addr = client.expect(r"\[CLIENT\] Phase36 addr=([0-9A-Fa-f:]{17})", timeout=20)
    addr = m_addr.group(1).decode()
    server.write(f"PHASE36_PEER {addr}\n")
    m_dir = server.expect(r"\[SERVER\] Phase36 directed noPeer=([01]) start=([01]) status=(\S+) rc=(-?\d+)", timeout=15)
    assert int(m_dir.group(1)) == 1, "directed start without setDirectedAddress must be InvalidParam"
    assert int(m_dir.group(2)) == 1, "directed high-duty start with the client address failed"
    client.write("PHASE36_CONNECT_GO\n")
    m_cc = client.expect(r"\[CLIENT\] Phase36 directed connected=([01])", timeout=10)
    assert int(m_cc.group(1)) == 1, "client did not connect to high-duty directed advertising"
    m_sc = server.expect(r"\[SERVER\] Phase36 directed connected=([01])", timeout=10)
    assert int(m_sc.group(1)) == 1, "server did not observe the directed-advertising connection"
    server.expect_exact("[SERVER] Phase36 oobLegacyReady", timeout=15)
    client.write("PHASE36_OOB_GO\n")
    m_cl = client.expect(
        r"\[CLIENT\] Phase36 oobLegacy found=([01]) connected=([01]) readOk=([01]) enc=([01]) "
        r"bond=([01]) authFired=([01]) authSuccess=([01])",
        timeout=40,
    )
    _assert_oob_reject(m_cl, "legacy OOB")
    m_sl = server.expect(
        r"\[SERVER\] Phase36 oobLegacy sawPeer=([01]) bonds=(\d+) authFired=([01]) authSuccess=([01])",
        timeout=40,
    )
    assert int(m_sl.group(1)) == 1 and int(m_sl.group(2)) == 0, "legacy OOB must connect and must not store a bond"
    assert (
        int(m_sl.group(3)) == 1 and int(m_sl.group(4)) == 0
    ), "legacy OOB must report authentication failure on the server"
    server.expect_exact("[SERVER] Phase36 oobScReady", timeout=15)
    client.write("PHASE36_OOB_SC_GO\n")
    m_csc = client.expect(
        r"\[CLIENT\] Phase36 oobSc found=([01]) connected=([01]) readOk=([01]) enc=([01]) "
        r"bond=([01]) authFired=([01]) authSuccess=([01])",
        timeout=40,
    )
    _assert_oob_reject(m_csc, "OOB-SC")
    m_ssc = server.expect(
        r"\[SERVER\] Phase36 oobSc sawPeer=([01]) bonds=(\d+) authFired=([01]) authSuccess=([01])",
        timeout=40,
    )
    assert int(m_ssc.group(1)) == 1 and int(m_ssc.group(2)) == 0, "OOB-SC must connect and must not store a bond"
    assert (
        int(m_ssc.group(3)) == 1 and int(m_ssc.group(4)) == 0
    ), "OOB-SC must report authentication failure on the server"
    client.expect_exact("[CLIENT] Phase36 done", timeout=15)
    server.expect_exact("[SERVER] Phase36 done", timeout=15)


def _phase_memory_release(server, client):
    MIN_FREED = 10240

    for tag, dut in [("SERVER", server), ("CLIENT", client)]:
        dut.expect(rf"\[{tag}\] Heap before release: \d+", timeout=30)
        dut.expect(rf"\[{tag}\] Heap after release: \d+", timeout=10)
        m = dut.expect(rf"\[{tag}\] Memory freed: (-?\d+) bytes", timeout=10)
        freed = int(m.group(1))
        assert freed >= MIN_FREED, f"{tag}: expected >= {MIN_FREED} bytes freed, got {freed}"
        dut.expect_exact(f"[{tag}] Memory release OK", timeout=10)
        dut.expect_exact(f"[{tag}] Reinit blocked OK", timeout=10)
        dut.expect_exact(f"[{tag}] All phases complete", timeout=10)


# ---------------------------------------------------------------------------
# Single test function — one upload, sub-results via record_property
# ---------------------------------------------------------------------------


def test_ble(dut, ci_job_id, record_property):
    server = dut[0]
    client = dut[1]

    name = "BLE_" + (ci_job_id if ci_job_id else rand_str4())
    LOGGER.info("BLE combined test name: %s", name)

    server.expect_exact("[SERVER] Device ready for name", timeout=120)
    client.expect_exact("[CLIENT] Device ready for name", timeout=120)
    server.expect_exact("[SERVER] Send name:")
    client.expect_exact("[CLIENT] Send name:")
    server.write(name)
    client.write(name)
    server.expect_exact(f"[SERVER] Name: {name}", timeout=10)
    client.expect_exact(f"[CLIENT] Target: {name}", timeout=10)

    # (phase_fn, skip_reason_if_falsy_return)
    PHASES = {
        1: (_phase_basic, server, client),
        2: (_phase_ble5_adv, server, client),  # skip if falsy
        3: (_phase_gatt_setup_connect, server, client),
        4: (_phase_gatt_rw, server, client),
        5: (_phase_notifications, server, client),
        6: (_phase_large_write, server, client),
        7: (_phase_descriptor_read_write, server, client),
        8: (_phase_write_no_response, server, client),
        9: (_phase_server_disconnect, server, client),
        10: (_phase_security, server, client),
        11: (_phase_ble5_phy_dle, client),  # skip if falsy
        12: (_phase_reconnect, server, client),
        13: (_phase_blestream, server, client),
        14: (_phase_l2cap, server, client),  # skip if falsy
        15: (_phase_introspection_and_permissions, server, client),
        16: (_phase_conninfo_and_params, server, client),
        17: (_phase_address_types, server, client),
        18: (_phase_authorization, server, client),
        19: (_phase_encrypted_perm, server, client),
        20: (_phase_adv_data_and_scan, server, client),
        21: (_phase_beacon_and_eddystone, server, client),
        22: (_phase_bond_and_whitelist, server, client),
        23: (_phase_error_paths_and_misc, server, client),
        24: (_phase_ble5_advanced, server, client),  # skip if falsy
        25: (_phase_hid_smoke, server, client),
        26: (_phase_ble5_legacy_over_ext, server, client),  # skip if falsy
        27: (_phase_ble5_legacy_plus_ext, server, client),  # skip if falsy
        28: (_phase_l2cap_bulk, server, client),  # skip if falsy
        29: (_phase_conn_params_reject, server, client),  # skip if falsy
        30: (_phase_deletebond_roundtrip, server, client),
        31: (_phase_nc_reject, server, client),
        32: (_phase_passkey_entry, server, client),
        33: (_phase_indication_busy_retry, server, client),
        34: (_phase_periodic_adv_lifecycle, server, client),  # skip if falsy
        35: (_phase_pending_send_edges, server, client),
        36: (_phase_directed_adv_and_oob, server, client),
        37: (_phase_memory_release, server, client),
    }

    SKIP_REASONS = {
        2: "BLE5 not supported",
        11: "BLE5 not supported",
        14: "L2CAP not supported",
        24: "BLE5 not supported",
        26: "BLE5 not supported",
        27: "BLE5 not supported",
        28: "L2CAP not supported",
        29: "conn-params pre-accept hook not supported (Bluedroid)",
        34: "BLE5 not supported",
    }

    passed, failed = [], []

    for num, (fn, *args) in PHASES.items():
        label = PHASE_LABELS[num]
        LOGGER.info("Running phase %d: %s", num, label)
        _start_phase(server, client, num)
        try:
            result = fn(*args)
            if num in SKIP_REASONS and not result:
                record_property(f"phase_{label}", f"SKIP: {SKIP_REASONS[num]}")
                LOGGER.info("SKIPPED: phase %d (%s)", num, label)
            else:
                passed.append(label)
                record_property(f"phase_{label}", "PASS")
                LOGGER.info("PASSED: phase %d (%s)", num, label)
        except Exception as e:
            failed.append((label, str(e)))
            record_property(f"phase_{label}", f"FAIL: {e}")
            LOGGER.error("FAILED: phase %d (%s): %s", num, label, e)
            if num <= 15:  # original code called pytest.fail on phases 1–15
                pytest.fail(f"phase {num} ({label}) failed: {e}")
            # Later phases run sequentially against one setup() state machine
            # per DUT; continuing after a failure only burns every remaining
            # timeout against dead state. Stop at the first failure.
            break

    summary = f"{len(passed)} passed, {len(failed)} failed out of {len(PHASE_LABELS)}"
    LOGGER.info("Summary: %s", summary)
    record_property("summary", summary)

    if failed:
        lines = [f"  {lbl}: {err}" for lbl, err in failed]
        pytest.fail(f"{len(failed)} phase(s) failed:\n" + "\n".join(lines))
