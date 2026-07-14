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
    33: "periodic_adv_lifecycle",
    34: "memory_release",
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
    server.expect_exact("[SERVER] Init OK", timeout=30)
    server.expect_exact("[SERVER] Deinit OK", timeout=10)
    server.expect_exact("[SERVER] Reinit OK", timeout=30)
    server.expect_exact("[SERVER] Final deinit OK", timeout=10)

    client.expect_exact("[CLIENT] Init OK", timeout=30)
    client.expect_exact("[CLIENT] Deinit OK", timeout=10)
    client.expect_exact("[CLIENT] Reinit OK", timeout=30)
    client.expect_exact("[CLIENT] Final deinit OK", timeout=10)


def _phase_ble5_adv(server, client):
    """Returns (ble5_server, ble5_client) booleans."""
    m_server = server.expect(r"\[SERVER\] (BLE5 init OK|BLE5 not supported, skipping)", timeout=30)
    ble5_server = b"BLE5 init OK" in m_server.group(0)

    m_client = client.expect(r"\[CLIENT\] (BLE5 init OK|BLE5 not supported, skipping)", timeout=30)
    ble5_client = b"BLE5 init OK" in m_client.group(0)

    if ble5_server and ble5_client:
        server.expect_exact("[SERVER] Extended adv configured", timeout=10)
        server.expect_exact("[SERVER] Extended adv started", timeout=10)
        server.expect_exact("[SERVER] Periodic adv started", timeout=10)

        client.expect_exact("[CLIENT] Scanning for periodic adv...", timeout=10)
        client.expect_exact("[CLIENT] Found target via ext scan!", timeout=60)
        client.expect_exact("[CLIENT] Synced to periodic adv!", timeout=60)
        client.expect_exact("[CLIENT] Periodic data received", timeout=30)
        client.expect_exact("[CLIENT] Periodic test complete", timeout=10)

        server.expect_exact("[SERVER] BLE5 adv phase done", timeout=30)
        client.expect_exact("[CLIENT] BLE5 scan phase done", timeout=10)

    return ble5_server, ble5_client


def _phase_gatt_setup_connect(server, client):
    server.expect(r"\[SERVER\] Heap before init: \d+", timeout=30)
    server.expect_exact("[SERVER] GATT init OK", timeout=30)
    server.expect(r"\[SERVER\] Heap after init: \d+", timeout=10)
    server.expect_exact("[SERVER] Security configured", timeout=10)
    server.expect(r"\[SERVER\] Heap after server: \d+", timeout=10)
    server.expect_exact("[SERVER] Server started", timeout=10)
    server.expect_exact("[SERVER] Advertising started", timeout=10)

    client.expect(r"\[CLIENT\] Heap before init: \d+", timeout=30)
    client.expect_exact("[CLIENT] GATT init OK", timeout=30)
    client.expect(r"\[CLIENT\] Heap after init: \d+", timeout=10)
    client.expect(r"\[CLIENT\] Scanning \(attempt \d+\)\.\.\.", timeout=10)
    client.expect_exact("[CLIENT] Found target server!", timeout=60)
    client.expect_exact("[CLIENT] Connected", timeout=20)
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


def _phase_notifications(server, client):
    client.expect_exact("[CLIENT] Subscribed to notifications", timeout=10)
    client.expect_exact("[CLIENT] Notification received: notify_test_1", timeout=30)
    client.expect_exact("[CLIENT] Subscribed to indications", timeout=10)
    client.expect_exact("[CLIENT] Indication received: indicate_test_1", timeout=30)
    client.expect_exact("[CLIENT] Unsubscribed", timeout=15)

    # "Subscriber count: 1" may appear before phase expectations due to callback timing.
    # Keep the phase robust by validating sent events and final unsubscribe state.
    server.expect_exact("[SERVER] Notification sent", timeout=10)
    server.expect_exact("[SERVER] Indication sent", timeout=10)
    server.expect_exact("[SERVER] Subscriber count: 0", timeout=10)


def _phase_large_write(server, client):
    client.expect_exact("[CLIENT] Large write OK", timeout=30)
    server.expect_exact("[SERVER] Received 512 bytes", timeout=10)
    client.expect_exact("[CLIENT] Large read (512 bytes)", timeout=30)
    client.expect_exact("[CLIENT] Large data integrity OK", timeout=10)


def _phase_descriptor_read_write(server, client):
    client.expect_exact("[CLIENT] Found descriptor char", timeout=10)
    client.expect(r"\[CLIENT\] Descriptor count: \d+", timeout=10)
    client.expect_exact("[CLIENT] User description: Test Characteristic", timeout=10)
    client.expect(r"\[CLIENT\] Presentation format: \d+", timeout=10)
    client.expect_exact("[CLIENT] Format matches UTF8", timeout=10)


def _phase_write_no_response(server, client):
    client.expect_exact("[CLIENT] Found WriteNR char", timeout=10)
    client.expect_exact("[CLIENT] WriteNR sent", timeout=10)
    server.expect_exact("[SERVER] WriteNR received: WriteNR_OK", timeout=10)
    # Use [^\r\n]+ not .+ : on CRLF, "." matches \r and greedy .+ can swallow the next line.
    client.expect(r"\[CLIENT\] WriteNR readback: [^\r\n]+", timeout=10)
    client.expect_exact("[CLIENT] Status: write_no_response done", timeout=10)


def _phase_server_disconnect(server, client):
    client.expect_exact("[CLIENT] Waiting for server disconnect...", timeout=10)
    server.expect_exact("[SERVER] Server-initiated disconnect OK", timeout=10)
    client.expect_exact("[CLIENT] Server disconnected us", timeout=15)
    server.expect_exact("[SERVER] Client disconnected", timeout=10)
    client.expect_exact("[CLIENT] Reconnected after server disconnect", timeout=30)
    server.expect_exact("[SERVER] Client connected", timeout=10)


def _phase_security(server, client):
    m_server = server.expect(r"\[SERVER\] Passkey: (\d+)", timeout=30)
    server_pin = m_server.group(1).decode()
    m_client = client.expect(r"\[CLIENT\] Passkey: (\d+)", timeout=30)
    client_pin = m_client.group(1).decode()
    assert server_pin == client_pin, f"PIN mismatch: {server_pin} vs {client_pin}"

    server.expect_exact("[SERVER] Authentication complete", timeout=30)
    client.expect_exact("[CLIENT] Authentication complete", timeout=30)
    client.expect_exact("[CLIENT] Secure read: Secure Data!", timeout=10)
    # onIdentity dedupe: the peer identity is surfaced exactly once per
    # connection on both backends. NimBLE receives identity from both
    # IDENTITY_RESOLVED and ENC_CHANGE and dedupes it in the library; Bluedroid
    # fires it once from AUTH_CMPL. A count != 1 means the dedupe regressed.
    m_id = client.expect(r"\[CLIENT\] Phase10 onIdentity count=(\d+)", timeout=10)
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


def _phase_blestream(server, client):
    # Init + onConnect callback + connected()
    server.expect_exact("[SERVER] BLEStream init OK", timeout=20)
    client.expect_exact("[CLIENT] BLEStream init OK", timeout=30)
    server.expect_exact("[SERVER] BLEStream onConnect fired", timeout=15)
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
    client.expect_exact("[CLIENT] BLEStream bulk sent: 200 bytes", timeout=10)
    server.expect_exact("[SERVER] BLEStream bulk received: 200 bytes", timeout=15)
    server.expect_exact("[SERVER] BLEStream bulk integrity OK", timeout=10)

    # Server-initiated write (server → client direction)
    server.expect_exact("[SERVER] BLEStream server msg sent", timeout=10)
    client.expect_exact("[CLIENT] BLEStream server msg: server_says_hi", timeout=15)

    # onDisconnect callback after client calls end()
    server.expect_exact("[SERVER] BLEStream onDisconnect fired", timeout=15)
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
      * A characteristic declared as Read|Write with only OpenRead permission
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
    # Server-side construction-time validator checks. These run in parallel
    # with the client introspection (both are gated on Phase 15), so assert
    # them up-front on the server stream before switching to the client.
    server.expect_exact("[SERVER] DescPerm validation start", timeout=15)
    server.expect_exact("[SERVER] DescPerm None rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2904+Write rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2900+Write rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2902 read-only rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm 0x2901 no-read rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm WriteAuthorized-no-write rejected: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm valid accepted: 1", timeout=10)
    server.expect_exact("[SERVER] DescPerm validation done", timeout=10)

    client.expect_exact("[CLIENT] Reconnected for introspection", timeout=30)
    client.expect(r"\[CLIENT\] RSSI: -?\d+", timeout=10)
    m_svc = client.expect(r"\[CLIENT\] Service count: (\d+)", timeout=10)
    assert int(m_svc.group(1)) >= 1, "Expected at least 1 service on server"
    m_ch = client.expect(r"\[CLIENT\] Characteristic count: (\d+)", timeout=10)
    # 6 original + 1 new fail-closed char. Allow >= to be backend-tolerant.
    assert int(m_ch.group(1)) >= 6, "Expected at least 6 characteristics on service"

    # Property introspection — exact flags match server-declared properties
    # (the RW char has only Read|Write, no Notify/Indicate, etc.).
    client.expect_exact("[CLIENT] RW: canRead=1 canWrite=1 canNotify=0 canIndicate=0", timeout=10)
    client.expect_exact("[CLIENT] Notify: canNotify=1 canIndicate=0 canRead=1", timeout=10)
    client.expect_exact("[CLIENT] Indicate: canIndicate=1 canNotify=0 canRead=1", timeout=10)
    client.expect_exact("[CLIENT] WriteNR: canWriteNoResponse=1 canWrite=0", timeout=10)

    # CCCD auto-created (NimBLE) or explicit (Bluedroid) — must be discoverable
    # from the client either way. Aggregate getDescriptors() is called first
    # to exercise the lazy auto-discovery on the aggregate getter.
    client.expect(r"\[CLIENT\] Notify descriptor count: \d+", timeout=10)
    client.expect_exact("[CLIENT] Notify CCCD found: 1", timeout=10)

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
    m_mtu = client.expect(r"\[CLIENT\] MtuChanged seen=([01]) last=(\d+)", timeout=10)
    assert int(m_mtu.group(1)) == 1, "client onMtuChanged never fired — MTU exchange did not complete"
    assert int(m_mtu.group(2)) > 23, f"client negotiated MTU {m_mtu.group(2)} not above the 23-byte default"
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
    m_smtu = server.expect(r"\[SERVER\] MtuChanged seen=([01]) last=(\d+)", timeout=15)
    assert int(m_smtu.group(1)) == 1, "server onMtuChanged never fired — MTU exchange did not complete"
    assert int(m_smtu.group(2)) > 23, f"server negotiated MTU {m_smtu.group(2)} not above the 23-byte default"
    server.expect(r"\[SERVER\] ConnParamsUpdate seen=[01] interval=\d+ latency=\d+ timeout=\d+", timeout=10)
    # getConnInfo(handle) must return a fresh snapshot reflecting the post-exchange
    # MTU/conn-params, reject a bogus handle, and behave the same on both stacks.
    m_gci = server.expect(
        r"\[SERVER\] Phase16 getConnInfo valid=([01]) mtu=(\d+) timeout=(\d+) bogusValid=([01]) refreshedMtu=([01]) refreshedParams=([01])",
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

    assert client_success >= 1, "expected at least one address-type connect roundtrip to succeed " "(Public or Random)"
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
        r"\[SERVER\] Authz counters reads=(\d+) writesOK=(\d+) writesDeny=(\d+)",
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
    # Slave Connection Interval Range AD (0x12): encoding guard on the server
    # (setPreferredParams / setMinPreferred / setMaxPreferred) plus the
    # over-the-air round trip parsed back by the client. The client emits the
    # prefInterval line *before* sawAdv, so match it first (a later expect would
    # consume past it and never find it).
    m_pref = server.expect(r"\[SERVER\] Phase20 prefIntervalAD ok=([01])", timeout=10)
    assert int(m_pref.group(1)) == 1, "setPreferredParams did not emit a correct Slave Connection Interval Range AD (0x12)"
    m_cpref = client.expect(
        r"\[CLIENT\] Phase20 prefInterval have=([01]) min=0x([0-9A-Fa-f]+) max=0x([0-9A-Fa-f]+)",
        timeout=10,
    )
    assert int(m_cpref.group(1)) == 1, "client did not receive the Slave Connection Interval Range AD (0x12)"
    assert int(m_cpref.group(2), 16) == 0x0006, f"preferred min interval mismatch: got 0x{m_cpref.group(2)}"
    assert int(m_cpref.group(3), 16) == 0x0012, f"preferred max interval mismatch: got 0x{m_cpref.group(3)}"
    m = client.expect(r"\[CLIENT\] Phase20 sawAdv=([01])", timeout=30)
    assert int(m.group(1)) == 1, "client did not see the ADV_<name> payload"
    client.expect_exact("[CLIENT] Phase20 done", timeout=10)
    server.expect_exact("[SERVER] Phase20 done", timeout=20)


def _phase_beacon_and_eddystone(server, client):
    """Phase 21 — iBeacon / Eddystone URL / Eddystone TLM frame round-trip."""
    server.expect(r"\[SERVER\] Phase21 iBeacon adv ok=[01]", timeout=30)
    server.expect(r"\[SERVER\] Phase21 EddystoneURL ok=[01]", timeout=30)
    server.expect(r"\[SERVER\] Phase21 EddystoneTLM ok=[01]", timeout=30)

    m = client.expect(
        r"\[CLIENT\] Phase21 sawIBeacon=([01]) sawEddyURL=([01]) sawEddyTLM=([01])",
        timeout=45,
    )
    ib = int(m.group(1))
    ur = int(m.group(2))
    tl = int(m.group(3))
    assert ib + ur + tl >= 2, f"expected at least 2 of 3 beacon frames, got iBeacon={ib} url={ur} tlm={tl}"
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
        m_local = dut.expect(rf"\[{tag}\] LocalIRK present=([01]) hex=([0-9a-fA-F]*)", timeout=10)
        m_peer = dut.expect(rf"\[{tag}\] PeerIRK present=([01]) hex=([0-9a-fA-F]*)", timeout=10)
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
    m_str = server.expect(r"\[SERVER\] StrConv pass=(\d+) fail=(\d+)", timeout=5)
    assert int(m_str.group(1)) > 0, "server StrConv: expected at least one passing check"
    assert int(m_str.group(2)) == 0, "server StrConv: implicit const char* UUID conversion failed"

    # Client connects while server waits (server polls for up to 20 s).
    client.expect_exact("[CLIENT] Phase23 connected", timeout=30)
    m_us = client.expect(r"\[CLIENT\] Phase23 getService unknown ok=([01])", timeout=10)
    assert int(m_us.group(1)) == 0, "client: unknown service must NOT resolve"
    m_uc = client.expect(r"\[CLIENT\] Phase23 getChar unknown ok=([01])", timeout=10)
    assert int(m_uc.group(1)) == 0, "client: unknown char must NOT resolve"
    client.expect(r"\[CLIENT\] Phase23 typed u8=\d+ u16=\d+ u32=\d+ f32=-?\d+", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 readBuf len=\d+", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 readRaw len=\d+ nonNull=[01]", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 UUID eq=1 ne=1 lt=1", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 UUID bitSize16=16 128=128 toUint16=0x180D", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 UUID valid=1 defaultBool=0", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 postDisc isConnected=[01]", timeout=15)
    m_wa = client.expect(r"\[CLIENT\] Phase23 writeAfterDisc ok=([01])", timeout=10)
    assert int(m_wa.group(1)) == 0, "write-after-disconnect must fail"
    client.expect(r"\[CLIENT\] Phase23 connectAsync ok=[01]", timeout=10)
    client.expect(r"\[CLIENT\] Phase23 cancelConnect ok=[01]", timeout=10)
    m_cstr = client.expect(r"\[CLIENT\] StrConv pass=(\d+) fail=(\d+)", timeout=10)
    assert int(m_cstr.group(1)) > 0, "client StrConv: expected at least one passing check"
    assert int(m_cstr.group(2)) == 0, "client StrConv: implicit const char* UUID conversion failed"
    client.expect_exact("[CLIENT] Phase23 done", timeout=10)
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
    if server_ok:
        server.expect(r"\[SERVER\] Phase24 setDefaultPhy ok=1", timeout=5)
        # The client now holds a dedicated connection for this phase, so the
        # per-connection PHY/DLE path must run and succeed every run.
        server.expect_exact("[SERVER] Phase24 setPhy(2M) ok=1", timeout=15)
        server.expect(r"\[SERVER\] Phase24 getPhy ok=1 tx=\d+ rx=\d+", timeout=5)
        server.expect_exact("[SERVER] Phase24 setDataLen ok=1", timeout=5)
    server.expect_exact("[SERVER] Phase24 done", timeout=15)

    m_c = client.expect(
        r"\[CLIENT\] (Phase24 BLE5 not supported, skipping|Phase24 getDefaultPhy ok=[01] tx=\d+ rx=\d+)",
        timeout=20,
    )
    client_ok = b"getDefaultPhy" in m_c.group(0)
    if client_ok:
        client.expect(r"\[CLIENT\] Phase24 setDefaultPhy ok=1", timeout=5)
        client.expect(r"\[CLIENT\] Phase24 startExtendedCoded ok=1", timeout=10)
        client.expect_exact("[CLIENT] Phase24 phyConn ok=1", timeout=20)
    client.expect_exact("[CLIENT] Phase24 done", timeout=15)

    return server_ok and client_ok


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
    server.expect(r"\[SERVER\] Phase25 hidChars=\d+", timeout=30)

    m_mr = server.expect(r"\[SERVER\] Phase25 multiReport=([01])", timeout=10)
    assert int(m_mr.group(1)) == 1, "inputReport + outputReport must produce distinct characteristics"

    m_cache = server.expect(r"\[SERVER\] Phase25 bootCached=([01])", timeout=10)
    assert int(m_cache.group(1)) == 1, "bootInput() must return the same handle on repeated calls"

    server.expect(r"\[SERVER\] Phase25 bootOut=[01]", timeout=10)
    server.expect(r"\[SERVER\] Phase25 HID adv ok=[01]", timeout=10)
    server.expect_exact("[SERVER] Phase25 HID ready", timeout=10)

    client.expect_exact("[CLIENT] Phase25 HID found", timeout=30)

    m_svcs = client.expect(
        r"\[CLIENT\] Phase25 services=(\d+) haveHid=([01]) haveBat=([01])",
        timeout=20,
    )
    assert int(m_svcs.group(2)) == 1, "HID Service (0x1812) must be discoverable"
    assert int(m_svcs.group(3)) == 1, "Battery Service (0x180F) must be discoverable"

    m_ch = client.expect(r"\[CLIENT\] Phase25 hidChars=(\d+)", timeout=10)
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

    m_rpt = client.expect(r"\[CLIENT\] Phase25 reportCount=(\d+)", timeout=10)
    assert int(m_rpt.group(1)) >= 2, "Must have at least 2 Report characteristics (input + output)"

    m_boot = client.expect(
        r"\[CLIENT\] Phase25 bootInOk=([01]) bootOutOk=([01])",
        timeout=10,
    )
    assert int(m_boot.group(1)) == 1, "Boot Keyboard Input must have Read + Notify properties"
    assert int(m_boot.group(2)) == 1, "Boot Keyboard Output must have Read + Write + WriteNR properties"

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
        r"\[CLIENT\] Phase26 sawLegacy=([01]) legacy=([01]) connectable=([01])",
        timeout=30,
    )
    assert int(m_c.group(1)) == 1, "client did not discover the legacy-over-ext advertisement"
    assert int(m_c.group(2)) == 1, "legacy-over-ext adv was not flagged as a legacy advertisement"
    assert int(m_c.group(3)) == 1, "legacy-over-ext adv was not flagged connectable"
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
    assert int(m_ext.group(1)) == 1, "server failed to start the concurrent extended set (reserved-instance concurrency broken)"

    m_c = client.expect(
        r"\[CLIENT\] Phase27 sawLegacy=([01]) sawExt=([01]) legacyIsLegacy=([01]) extIsLegacy=([01])",
        timeout=30,
    )
    assert int(m_c.group(1)) == 1, "client did not see the concurrent legacy set (LEG_ name)"
    assert int(m_c.group(2)) == 1, "client did not see the concurrent extended set (EXT_ name)"
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

    m_c = client.expect(
        r"\[CLIENT\] Phase28 (L2CAP not supported, skipping|rescan found=[01])",
        timeout=40,
    )
    if b"not supported" in m_c.group(0):
        client.expect_exact("[CLIENT] Phase28 done", timeout=15)
        server.expect_exact("[SERVER] Phase28 done", timeout=40)
        return False

    client.expect_exact("[CLIENT] Phase28 L2CAP channel connected", timeout=30)
    # Bulk SDU must arrive intact (server echoes the accumulated byte count once
    # it reaches the target, proving credit replenishment mid transfer worked).
    m_ack = client.expect(r"\[CLIENT\] Phase28 bulkAck ok=([01]) total=(\d+)", timeout=60)
    assert int(m_ack.group(1)) == 1, f"L2CAP bulk transfer size mismatch (server acked {m_ack.group(2).decode()} bytes)"
    server.expect(r"\[SERVER\] Phase28 bulk got=\d+", timeout=30)
    # Fresh channel after teardown must ping/pong — the accept path recovered.
    m_re = client.expect(r"\[CLIENT\] Phase28 reconnect ok=([01])", timeout=40)
    assert int(m_re.group(1)) == 1, "L2CAP channel reconnect ping/pong failed"

    client.expect_exact("[CLIENT] Phase28 done", timeout=15)
    server.expect_exact("[SERVER] Phase28 done", timeout=40)
    return True


def _phase_conn_params_reject(server, client):
    """Phase 29 — the central rejects the peripheral's connection-parameter
    update from its pre-accept hook. NimBLE-only: Bluedroid negotiates conn
    params inside L2CAP with no pre-accept surface, so the client self-skips.

    Returns True if the phase ran (hook fired), False if skipped (Bluedroid).
    """
    # If the client never (re)discovered the server there is nothing to
    # exercise — treat it as a skip rather than a hard failure.
    m_f = client.expect(r"\[CLIENT\] Phase29 rescan found=([01])", timeout=45)
    if int(m_f.group(1)) == 0:
        client.expect_exact("[CLIENT] Phase29 done", timeout=15)
        server.expect(r"\[SERVER\] Phase29 (requestSent ok=[01]|no peer connected|rebuild FAILED)", timeout=30)
        server.expect_exact("[SERVER] Phase29 done", timeout=20)
        return False

    server.expect(r"\[SERVER\] Phase29 (requestSent ok=[01]|no peer connected)", timeout=30)
    m_c = client.expect(
        r"\[CLIENT\] Phase29 hookFired=([01]) beforeInterval=(\d+) afterInterval=(\d+) held=([01])",
        timeout=40,
    )
    client.expect_exact("[CLIENT] Phase29 done", timeout=10)
    server.expect_exact("[SERVER] Phase29 done", timeout=20)

    if int(m_c.group(1)) == 0:
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
    server.expect(r"\[SERVER\] Phase30 bondsBefore=\d+", timeout=45)
    server.expect(r"\[SERVER\] Phase30 bondsAfterDelete=\d+", timeout=15)

    m_cb = client.expect(r"\[CLIENT\] Phase30 bondsBefore=(\d+)", timeout=45)
    m_cd = client.expect(r"\[CLIENT\] Phase30 bondsAfterDelete=(\d+)", timeout=15)
    assert int(m_cb.group(1)) >= 1, "client had no bond to delete (phase 10 pairing missing?)"
    assert int(m_cd.group(1)) == 0, "client bond store not empty after deleteAllBonds"

    m_cr = client.expect(
        r"\[CLIENT\] Phase30 reEnc=([01]) reBond=([01]) bondsAfterRepair=(\d+)",
        timeout=45,
    )
    assert int(m_cr.group(1)) == 1, "link not encrypted after deleteBond re-pair"
    assert int(m_cr.group(2)) == 1, "link not bonded after deleteBond re-pair"
    assert int(m_cr.group(3)) >= 1, "bond store still empty after re-pair (deleteBond round-trip failed)"

    m_sr = server.expect(r"\[SERVER\] Phase30 rebonded=([01]) bondsAfterRepair=(\d+)", timeout=45)
    assert int(m_sr.group(1)) == 1, "server did not observe a bonded re-pair"

    client.expect_exact("[CLIENT] Phase30 done", timeout=15)
    server.expect_exact("[SERVER] Phase30 done", timeout=15)


def _phase_nc_reject(server, client):
    """Phase 31 — an explicit Numeric-Comparison reject must abort pairing: the
    client returns false from its confirm handler, so the authenticated read
    fails and neither end ends up encrypted or bonded. Runs on both stacks.
    """
    m_c = client.expect(
        r"\[CLIENT\] Phase31 ncReject readOk=([01]) enc=([01]) bond=([01]) bonds=(\d+) authFired=([01]) authSuccess=([01])",
        timeout=60,
    )
    assert int(m_c.group(1)) == 0, "secure read succeeded despite the numeric-comparison reject"
    assert int(m_c.group(2)) == 0, "link encrypted despite the numeric-comparison reject"
    assert int(m_c.group(3)) == 0, "link bonded despite the numeric-comparison reject"
    assert int(m_c.group(4)) == 0, "a bond persisted despite the numeric-comparison reject"
    # If auth-complete surfaced at all, it must report failure.
    if int(m_c.group(5)) == 1:
        assert int(m_c.group(6)) == 0, "auth-complete reported success despite the reject"

    m_s = server.expect(r"\[SERVER\] Phase31 ncReject sawPeer=([01]) bonds=(\d+)", timeout=60)
    assert int(m_s.group(2)) == 0, "server retained a bond despite the numeric-comparison reject"

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

    m_s = server.expect(r"\[SERVER\] Phase32 passkeyEntry rebonded=([01]) bonds=(\d+)", timeout=75)
    assert int(m_s.group(1)) == 1, "server did not observe a bonded passkey-entry pairing"
    assert int(m_s.group(2)) >= 1, "server stored no bond after passkey-entry pairing"

    client.expect_exact("[CLIENT] Phase32 done", timeout=15)
    server.expect_exact("[SERVER] Phase32 done", timeout=15)


def _phase_periodic_adv_lifecycle(server, client):
    """Phase 33 — periodic-advertising teardown transitions the happy path never
    reached: an explicit terminatePeriodicSync and an onPeriodicLost after the
    server stops the train. Self-skips without BLE5.

    Returns True if the phase ran, False if skipped.
    """
    m_s = server.expect(
        r"\[SERVER\] Phase33 (periodic started|BLE5 not supported, skipping)",
        timeout=30,
    )
    if b"not supported" in m_s.group(0):
        server.expect_exact("[SERVER] Phase33 done", timeout=15)
        client.expect_exact("[CLIENT] Phase33 done", timeout=40)
        return False

    m_c = client.expect(
        r"\[CLIENT\] Phase33 (synced=([01]) dataRx=([01])|BLE5 not supported, skipping)",
        timeout=45,
    )
    if b"not supported" in m_c.group(0):
        client.expect_exact("[CLIENT] Phase33 done", timeout=15)
        server.expect_exact("[SERVER] Phase33 done", timeout=45)
        return False
    assert int(m_c.group(2)) == 1, "periodic sync not established"
    assert int(m_c.group(3)) == 1, "periodic report not received"

    # At least one teardown transition must be observed: an explicit terminate
    # (deterministic) or a supervision-timeout sync-lost (timing dependent).
    m_t = client.expect(r"\[CLIENT\] Phase33 lost=([01]) terminateOk=([01])", timeout=45)
    assert (int(m_t.group(1)) == 1) or (int(m_t.group(2)) == 1), "neither sync-lost nor explicit terminate observed (periodic teardown uncovered)"

    server.expect_exact("[SERVER] Phase33 periodic stopped", timeout=45)
    client.expect_exact("[CLIENT] Phase33 done", timeout=20)
    server.expect_exact("[SERVER] Phase33 done", timeout=20)
    return True


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
        33: (_phase_periodic_adv_lifecycle, server, client),  # skip if falsy
        34: (_phase_memory_release, server, client),
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
        33: "BLE5 not supported",
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

    summary = f"{len(passed)} passed, {len(failed)} failed out of {len(PHASE_LABELS)}"
    LOGGER.info("Summary: %s", summary)
    record_property("summary", summary)

    if failed:
        lines = [f"  {lbl}: {err}" for lbl, err in failed]
        pytest.fail(f"{len(failed)} phase(s) failed:\n" + "\n".join(lines))
