"""
LE Audio (GAF) hardware validation.

Split out of the combined `ble` suite so that test stays focused on core
GATT/GAP while this one owns the LE Audio stack: the ISO transport, BAP
unicast/broadcast data plane, the LC3 codec loopback, the control profiles
(VCP/MICP/MCP/CCP via CAP), the top-level identity profiles (TMAP/GMAP), the
Hearing Access Service, stereo broadcast, the BAP Broadcast Assistant (BASS
control point) and the CAP unicast-to-broadcast handover.

Phases run sequentially on two DUTs (server + client). Single test function,
one upload; each phase recorded via record_property so sub-results appear
individually in the final report.

Every phase self-skips when the relevant feature is not compiled in, so the
suite runs unchanged on non-audio silicon. The data-plane phases only truly
exercise LE Audio on a dual-audio esp32s31 <-> esp32s31 bench.
"""

import logging

import pytest

from conftest import rand_str4

LOGGER = logging.getLogger(__name__)

PHASE_LABELS = {
    1: "audio_lifecycle",
    2: "iso_cis",
    3: "iso_bis",
    4: "bap_unicast",
    5: "bap_broadcast",
    6: "lc3_loopback",
    7: "control_profiles",
    8: "top_profiles",
    9: "hearing_aid",
    10: "stereo_broadcast",
    11: "broadcast_assistant",
    12: "cap_handover",
    13: "bcast_stop_race",
    14: "cap_bc_interleave",
    15: "start_retry_refused",
    16: "security_policy",
    17: "iso_handle_gates",
    18: "mic_mute_disabled",
    19: "memory_release",
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


def _release_server(server):
    """End the server's wait loop once the client has produced its result.

    Timeouts on the DUT remain as ceilings; this only cuts idle time after
    the client-driven work is already done.
    """
    server.write("PHASE_RELEASE\n")


def _phase_audio_lifecycle(server, client):
    """LE Audio engine bring-up + plain-GATT coexistence (server-driven).

    Returns False (SKIP) when the LE Audio engine is not compiled into the build.
    """
    mp = server.expect(r"\[SERVER\] (Audio purePeriph ready=([01])|Audio not supported, skipping)", timeout=30)
    if b"not supported" in mp.group(0):
        client.expect_exact("[CLIENT] Audio not supported, skipping", timeout=20)
        server.expect_exact("[SERVER] Phase1 audio done", timeout=10)
        client.expect_exact("[CLIENT] Phase1 audio done", timeout=20)
        return False
    assert int(mp.group(2)) == 1, "pure-audio peripheral failed to start advertising"
    client.write("PHASE1_PURE_GO\n")
    mps = server.expect(
        r"\[SERVER\] Audio purePeriph link=([01]) vol=([01]) disc=([01]) end=([01]) busyGuard=([01])",
        timeout=45,
    )
    mpc = client.expect(
        r"\[CLIENT\] Audio purePeriph disc=([01]) write=([01]) down=([01])",
        timeout=30,
    )
    assert int(mps.group(5)) == 1, "audio.begin() while advertising did not enforce the active-GAP guard"
    assert (
        int(mps.group(1)) == 1 and int(mpc.group(1)) == 1
    ), "pure-audio peripheral without BLEServer failed to connect/discover"
    assert (
        int(mps.group(2)) == 1 and int(mpc.group(2)) == 1
    ), "inline VCP write inside onDiscovered did not land on pure-audio peripheral"
    assert int(mps.group(3)) == 1 and int(mpc.group(3)) == 1, "peripheral audio.disconnect() failed without BLEServer"
    assert int(mps.group(4)) == 1, "audio.end() failed after pure-audio peripheral session"

    msh = server.expect(
        r"\[SERVER\] Audio sameHostReinit ok=([01]) badStart=([01]) fastEnd=([01]) ms=(\d+)",
        timeout=20,
    )
    assert int(msh.group(1)) == 1, "same-host audio.end()->begin()->start() without BLE.end() failed"
    assert int(msh.group(2)) == 1, "BroadcastSource::start(255) did not reject an invalid advertising instance"
    assert (
        int(msh.group(3)) == 1
    ), f"audio.end() after failed BroadcastSource::start() stalled ({msh.group(4).decode()} ms)"

    mg = server.expect(r"\[SERVER\] Audio GATT remove=(\d+) guarded=(\d+)", timeout=30)
    assert int(mg.group(1)) == 1, "plain-GATT service removal failed while audio owned the GATT table"
    assert int(mg.group(2)) == 1, "late GATT restaging was not guarded while audio owned the table"
    mcoex = server.expect(r"\[SERVER\] Audio coex ready=([01])", timeout=15)
    assert int(mcoex.group(1)) == 1, "coexistence advertising failed to start"
    client.write("PHASE1_COEX_GO\n")
    me = server.expect(r"\[SERVER\] Audio callback end=(\d+) activeEnded=(\d+) admission=(\d+)", timeout=40)
    assert int(me.group(1)) == 1, "client did not enter the callback-safe BLE.end() path"
    assert int(me.group(2)) == 1, "BLE.end() did not tear down the active audio controller"
    assert int(me.group(3)) == 1, "retained PAST calls were not rejected after BLE.end()"
    mr = server.expect(r"\[SERVER\] Audio reinit begin=(\d+) start=(\d+)", timeout=20)
    assert int(mr.group(1)) == 1, "audio reinit (init) failed after callback-safe BLE.end()"
    assert int(mr.group(2)) == 1, "audio reinit (commit) failed after callback-safe BLE.end()"
    mgf = server.expect(r"\[SERVER\] Audio gattfirst live=(\d+) guard=(\d+) remove=(\d+)", timeout=30)
    assert int(mgf.group(1)) == 1, "GATT-first classic services were not adopted into the audio commit"
    assert int(mgf.group(2)) == 1, "late restage was not guarded after a GATT-first commit"
    assert int(mgf.group(3)) == 1, "adopted classic service could not be removed"
    m = server.expect(
        r"\[SERVER\] (Audio lifecycle OK|Audio lifecycle FAILED begin=(\d+) end=(\d+))",
        timeout=30,
    )
    cr = client.expect(r"\[CLIENT\] Audio callback read: (\d+)", timeout=30)
    assert int(cr.group(1)) == 1, "client did not find/read the plain-GATT service committed with audio"
    client.expect(r"\[CLIENT\] (Audio lifecycle OK|Audio not supported, skipping)", timeout=20)
    assert b"FAILED" not in m.group(0), f"audio lifecycle failed: begin={m.group(2)} end={m.group(3)}"
    server.expect_exact("[SERVER] Phase1 audio done", timeout=20)
    client.expect_exact("[CLIENT] Phase1 audio done", timeout=10)
    return True


def _phase_iso_cis(server, client):
    """BLEIso CIS transport integrity (server = Peripheral, client = Central).

    The CIS is bidirectional: the Central streams to the Peripheral and the
    Peripheral streams back. The client then repeats the transport on the ISO
    host the LE Audio engine owns, releases that host with the CIG still
    allocated, and brings up one more CIS on the host it re-claims. Returns
    False (SKIP) when host ISO is not compiled into either build (the only rig
    that truly runs this is a dual-ISO esp32s31<->esp32s31 pair).
    """
    msh = client.expect(
        r"\[CLIENT\] IsoCis (not supported|sharedhost=(\d+) sharedcis=(\d+) sharedsdu=(\d+) "
        r"groupdrop=(\d+) hostbusy=(\d+) rehandover=(\d+))",
        timeout=120,
    )
    client_iso = b"not supported" not in msh.group(0)
    mc = None
    if client_iso:
        mc = client.expect(
            r"\[CLIENT\] IsoCis result connected=(\d+) sent=(\d+) received=(\d+) lifecycle=(\d+)",
            timeout=30,
        )
    _release_server(server)
    ms = server.expect(
        r"\[SERVER\] IsoCis (not supported|result connected=(\d+) received=(\d+) returned=(\d+) "
        r"lifecycle=(\d+) connects=(\d+))",
        timeout=60,
    )
    server.expect_exact("[SERVER] Phase2 iso_cis done", timeout=60)
    client.expect_exact("[CLIENT] Phase2 iso_cis done", timeout=60)

    if b"not supported" in ms.group(0) or not client_iso:
        return False

    received = int(ms.group(3))
    returned = int(ms.group(4))
    sent = int(mc.group(2))
    returned_rx = int(mc.group(3))
    assert int(ms.group(5)) == 1, "server BLE.end() did not release standalone ISO ownership"
    assert int(mc.group(4)) == 1, "client BLE.end() did not release standalone ISO ownership"
    assert sent > 0, "CIS Central sent no SDUs"
    assert received > 0, f"CIS Peripheral received no SDUs (Central sent {sent})"
    assert returned > 0, "CIS Peripheral sent no SDUs back"
    assert returned_rx > 0, f"CIS Central received no SDUs back (Peripheral sent {returned})"

    # Shared-host handover: raw CIS on the ISO host the LE Audio engine owns,
    # then the engine releases that host while the CIG is still allocated.
    assert int(msh.group(2)) == 1, "LE Audio engine did not claim the ISO host for the shared-host stage"
    assert int(msh.group(3)) == 1, "CIS did not come up on the audio-owned ISO host"
    assert int(msh.group(4)) > 0, "no SDUs were sent on the audio-owned ISO host"
    assert int(msh.group(5)) == 1, "iso.end() did not drop the CIG created on the shared host"
    # hostbusy is informational: this IDF refuses to release the shared host
    # while a CIG slot is allocated (bt_le_iso_report_busy() counts them).
    assert int(msh.group(7)) == 1, (
        "connectCis() came back empty after the audio engine released the shared ISO host: "
        "a CIG handle survived the release and was terminated on freed state"
    )
    assert int(ms.group(6)) >= 3, f"server accepted {int(ms.group(6))} CISes; expected one per client handover stage"
    return True


def _phase_iso_bis(server, client):
    """BLEIso BIS transport integrity (server = Broadcaster, client = Receiver).

    Returns False (SKIP) when host ISO or BLE5 is not compiled into either build.
    """
    mc = client.expect(
        r"\[CLIENT\] IsoBis (not supported|result synced=(\d+) received=(\d+) lifecycle=(\d+))", timeout=90
    )
    _release_server(server)
    ms = server.expect(
        r"\[SERVER\] IsoBis (not supported|result connected=(\d+) sent=(\d+) lifecycle=(\d+))", timeout=60
    )
    server.expect_exact("[SERVER] Phase3 iso_bis done", timeout=40)
    client.expect_exact("[CLIENT] Phase3 iso_bis done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    sent = int(ms.group(3))
    received = int(mc.group(3))
    assert int(ms.group(4)) == 1, "server BLE.end() did not release standalone ISO over a live BIG"
    assert int(mc.group(4)) == 1, "client BLE.end() did not release standalone ISO over a live BIG"
    assert sent > 0, "BIS Broadcaster streamed no SDUs"
    assert received > 0, f"BIS Receiver received no SDUs (Broadcaster sent {sent})"
    return True


def _phase_bap_unicast(server, client):
    """BAP unicast transport integrity (server = Unicast Server, client = Unicast Client).

    The client runs the full BAP setup and streams transparent SDUs to the
    server's sink ASE over a CIS. Returns False (SKIP) when the LE Audio engine
    is not compiled into either build (only a dual-audio esp32s31 pair runs it).
    """
    ms = server.expect(r"\[SERVER\] BapUnicast (not supported|ready)", timeout=120)
    if b"not supported" in ms.group(0):
        client.expect(
            r"\[CLIENT\] BapUnicast (not supported|result streaming=(\d+) sent=(\d+) presets=(\d+))", timeout=120
        )
        server.expect_exact("[SERVER] Phase4 bap_unicast done", timeout=40)
        client.expect_exact("[CLIENT] Phase4 bap_unicast done", timeout=40)
        return False

    # The server's unicast advertising is up. Only now let the client scan and
    # connect, so it cannot latch the previous phase's advertising.
    client.write("PHASE4_GO\n")

    m_disc = client.expect(r"\[CLIENT\] BapUnicast disconnect ok=([01]) down=([01])", timeout=20)
    assert int(m_disc.group(1)) == 1, "BLEAudio::disconnect() failed for a live audio link"
    assert int(m_disc.group(2)) == 1, "BLEAudio::disconnect() did not deliver onDisconnected()"
    m_cycles = client.expect(r"\[CLIENT\] BapUnicast disconnectCycles=(\d+)", timeout=40)
    assert int(m_cycles.group(1)) == 2, "repeated audio connect/disconnect cycles failed or leaked"
    client.expect(r"\[CLIENT\] BapUnicast peripheralDisconnect armed h=\d+", timeout=30)
    server.write("SRV_DISC_GO\n")
    m_pdisc_s = server.expect(r"\[SERVER\] BapUnicast peripheralDisconnect ok=([01]) down=([01])", timeout=30)
    assert int(m_pdisc_s.group(1)) == 1 and int(m_pdisc_s.group(2)) == 1, "server peripheral-link disconnect failed"
    m_pdisc_c = client.expect(r"\[CLIENT\] BapUnicast peripheralDisconnect down=([01])", timeout=20)
    assert int(m_pdisc_c.group(1)) == 1, "client did not observe the server-initiated disconnect"
    ms2 = server.expect(r"\[SERVER\] BapUnicast result received=(\d+)", timeout=120)
    mc = client.expect(
        r"\[CLIENT\] BapUnicast (not supported|result streaming=(\d+) sent=(\d+) presets=(\d+) "
        r"restarted=(\d+) resent=(\d+) target=(\d+) other=(\d+))",
        timeout=120,
    )
    server.expect_exact("[SERVER] Phase4 bap_unicast done", timeout=40)
    client.expect_exact("[CLIENT] Phase4 bap_unicast done", timeout=40)

    if b"not supported" in mc.group(0):
        return False

    received = int(ms2.group(1))
    streaming, sent, presets, restarted, resent, target_accept, other_reject = (int(mc.group(i)) for i in range(2, 9))
    assert presets == 1, "Unicast Client did not find LC3_16_2_1 in the server's sink PACS"
    assert streaming == 1, "unicast stream never reached Streaming on the client"
    assert sent > 0, "Unicast Client sent no SDUs"
    assert received > 0, f"Unicast Server received no SDUs (Client sent {sent})"
    assert (
        restarted == 1 and resent > 0
    ), f"a second unicast session on the same peer failed (restarted={restarted}, resent={resent})"
    assert (
        target_accept == 1 and other_reject == 1
    ), f"target-aware connect mismatch (target={target_accept}, other_reject={other_reject})"
    return True


def _phase_bap_broadcast(server, client):
    """BAP broadcast transport integrity (server = Broadcast Source, client = Broadcast Sink).

    The server announces an Auracast broadcast (ext + periodic adv + BASE) and
    streams transparent SDUs over a BIG; the client scans, PA-syncs, syncs the
    BIG, and counts SDUs received. Returns False (SKIP) when the LE Audio engine
    is not compiled into either build (only a dual-audio esp32s31 pair runs it).
    """
    mc = client.expect(
        r"\[CLIENT\] BapBroadcast (not supported|result received=(\d+) base=(\d+) lifecycle=(\d+))", timeout=120
    )
    _release_server(server)
    ms = server.expect(r"\[SERVER\] BapBroadcast (not supported|result sent=(\d+) lifecycle=(\d+))", timeout=90)
    server.expect_exact("[SERVER] Phase5 bap_broadcast done", timeout=40)
    client.expect_exact("[CLIENT] Phase5 bap_broadcast done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    sent = int(ms.group(2))
    received = int(mc.group(2))
    base = int(mc.group(3))
    assert int(ms.group(3)) == 1, "server BLE.end() did not release audio over a live BIG"
    assert int(mc.group(4)) == 1, "client BLE.end() did not release audio over a live BIG"
    assert sent > 0, "Broadcast Source streamed no SDUs"
    assert base == 1, "Broadcast Sink never reported the source's BASE (onBaseReceived)"
    assert received > 0, f"Broadcast Sink received no SDUs (Source sent {sent})"
    return True


def _phase_lc3_loopback(server, client):
    """End-to-end LC3 audio path (client Recorder -> CIS -> server Player).

    Round 0 streams LC3_16_2_1 for ~4 s; round 1 soaks LC3_441_2_1 for ~15 s
    (framed SDUs, 10884 us interval). The client also injects a TX stall in
    round 0 (blocked controller releases must resync via txStalls and resume)
    and checks per-round PACS preset coverage via onDiscovered. The server
    runs a Goertzel tone detector over the decoded PCM (the client sends a
    sine at exactly fs/16): most of the energy must sit in the tone bin,
    proving the received audio is the transmitted waveform and not just
    loud noise.

    Returns False (SKIP) when the LC3 codec (esp_audio_codec) is not compiled
    into either build; only a dual-audio esp32s31 pair with the codec runs it.
    """
    mprobe = client.expect(
        r"\[CLIENT\] (LC3Loopback not supported|LC3Stall result stalled=(\d+) resumed=(\d+))", timeout=120
    )
    if b"not supported" in mprobe.group(0):
        _release_server(server)
        ms = server.expect(r"\[SERVER\] LC3Loopback not supported", timeout=60)
        server.expect_exact("[SERVER] Phase6 lc3_loopback done", timeout=40)
        client.expect_exact("[CLIENT] Phase6 lc3_loopback done", timeout=40)
        return False
    mstall = mprobe
    mpre0 = client.expect(r"\[CLIENT\] LC3Preset round=0 ok=(\d+)", timeout=30)
    mc0 = client.expect(r"\[CLIENT\] LC3Loopback result streaming=(\d+) sent=(\d+) errors=(\d+)", timeout=60)
    _release_server(server)
    ms0 = server.expect(
        r"\[SERVER\] LC3Loopback result samples=(\d+) meanamp=(\d+) plc=(\d+) underruns=(\d+) tone=(\d+)", timeout=120
    )
    mpre1 = client.expect(r"\[CLIENT\] LC3Preset round=1 ok=(\d+)", timeout=120)
    mc1 = client.expect(r"\[CLIENT\] LC3Loopback44 result streaming=(\d+) sent=(\d+) errors=(\d+)", timeout=120)
    _release_server(server)
    ms1 = server.expect(
        r"\[SERVER\] LC3Loopback44 result samples=(\d+) meanamp=(\d+) plc=(\d+) underruns=(\d+) tone=(\d+)", timeout=120
    )
    server.expect_exact("[SERVER] Phase6 lc3_loopback done", timeout=40)
    client.expect_exact("[CLIENT] Phase6 lc3_loopback done", timeout=40)

    stalled, resumed = int(mstall.group(2)), int(mstall.group(3))
    assert stalled == 1, "TX-stall injection did not resync (txStalls did not advance)"
    assert resumed == 1, "TX stream did not resume after the stall"
    assert int(mpre0.group(1)) == 1, "round-0 PACS preset not published by the server"
    assert int(mpre1.group(1)) == 1, "round-1 (44.1 kHz) PACS preset not published by the server"
    for label, mc, ms in (("16k", mc0, ms0), ("44k", mc1, ms1)):
        samples = int(ms.group(1))
        meanamp = int(ms.group(2))
        underruns = int(ms.group(4))
        tone = int(ms.group(5))
        streaming = int(mc.group(1))
        encoded = int(mc.group(2))
        errors = int(mc.group(3))
        LOGGER.info(
            "lc3_loopback %s: sent=%d errors=%d samples=%d meanamp=%d plc=%d underruns=%d tone=%d/1000",
            label,
            encoded,
            errors,
            samples,
            meanamp,
            int(ms.group(3)),
            underruns,
            tone,
        )
        assert streaming == 1, f"unicast stream never reached Streaming on the client ({label})"
        assert encoded > 0, f"Recorder sent no LC3 SDUs ({label}, {errors} send errors)"
        assert samples > 0, f"Player decoded no PCM samples ({label})"
        assert underruns == 0, f"Player underran {underruns} times ({label}); the PCM sink is not paced to the stream"
        assert meanamp > 500, f"decoded audio too quiet (meanamp={meanamp}, {label}); tone did not survive LC3"
        assert (
            tone >= 700
        ), f"decoded audio is not the transmitted tone (tone-bin share {tone}/1000, {label}); waveform corrupted"
    return True


def _phase_control_profiles(server, client):
    """LE Audio control profiles over one ACL: CAP + VCP + MICP + MCP + CCP.

    The server exposes a full headset-like control surface (CAS+CSIS, VCS,
    MICS, MCS backed by the reference media player, GTBS); the client connects
    once and drives every profile end-to-end. Both client and server outcomes
    are strictly asserted for all five profiles.
    """
    ma = client.expect(
        r"\[CLIENT\] (AsyncError result vcpQ=(\d+) micQ=(\d+) vcpTerms=(\d+) vcpNotConn=(\d+) "
        r"micTerms=(\d+) micNotConn=(\d+) quiet=(\d)(\d+)|ControlProfiles not supported)",
        timeout=120,
    )
    if b"not supported" in ma.group(0):
        mc = ma
    else:
        mc = client.expect(
            r"\[CLIENT\] ControlProfiles (not supported|result cap=(\d+) vcp=(\d+) mic=(\d+) media=(\d+) call=(\d+))",
            timeout=60,
        )
    _release_server(server)
    ms = server.expect(
        r"\[SERVER\] ControlProfiles (not supported|result volwrites=(\d+) micmutes=(\d+) media=(\d+) calls=(\d+))",
        timeout=120,
    )
    server.expect_exact("[SERVER] Phase7 control_profiles done", timeout=40)
    client.expect_exact("[CLIENT] Phase7 control_profiles done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    vol_writes = int(ms.group(2))
    mic_mutes = int(ms.group(3))
    media_cmds = int(ms.group(4))
    call_reqs = int(ms.group(5))
    cap_disc = int(mc.group(2))
    vcp_ok = int(mc.group(3))
    mic_ok = int(mc.group(4))
    media_ok = int(mc.group(5))
    call_ok = int(mc.group(6))
    LOGGER.info(
        "control_profiles: cap=%d vcp=%d/%d mic=%d/%d media=%d/%d call=%d/%d",
        cap_disc,
        vcp_ok,
        vol_writes,
        mic_ok,
        mic_mutes,
        media_ok,
        media_cmds,
        call_ok,
        call_reqs,
    )

    assert cap_disc == 1, "CAP initiator did not discover the acceptor's CAS"
    assert vcp_ok == 1 and vol_writes >= 1, f"VCP round-trip failed (client={vcp_ok}, server_writes={vol_writes})"
    assert mic_ok == 1 and mic_mutes >= 1, f"MICP mute round-trip failed (client={mic_ok}, server_mutes={mic_mutes})"
    assert (
        media_ok == 1 and media_cmds >= 1
    ), f"MCP Play round-trip failed (client={media_ok}, server_cmds={media_cmds})"
    assert (
        call_ok == 1 and call_reqs >= 1
    ), f"CCP Originate round-trip failed (client={call_ok}, server_calls={call_reqs})"
    vcp_q, mic_q = int(ma.group(2)), int(ma.group(3))
    vcp_terms, mic_terms = int(ma.group(4)), int(ma.group(6))
    vcp_quiet, mic_quiet = int(ma.group(8)), int(ma.group(9))
    LOGGER.info(
        "async_error: vcpQ=%d micQ=%d vcpTerms=%d(vcpNotConn=%s) micTerms=%d(micNotConn=%s) quiet=%d%d",
        vcp_q,
        mic_q,
        vcp_terms,
        ma.group(5).decode(),
        mic_terms,
        ma.group(7).decode(),
        vcp_quiet,
        mic_quiet,
    )
    assert vcp_q == 1 and mic_q == 1, "async-error injection did not queue both control writes"
    assert vcp_terms == 1 and mic_terms == 1, "queued control write did not terminate exactly once after disconnect"
    assert vcp_quiet == 1 and mic_quiet == 1, "duplicate terminal arrived after quiescence; exactly-once violated"
    return True


def _phase_top_profiles(server, client):
    """Top-level profile identity: TMAP and GMAP round-trips.

    The server publishes TMAS (CallTerminal|UnicastMediaReceiver) and GMAS
    (UnicastGameTerminal with the UGT Sink feature). The client discovers both
    and asserts the role and feature bitmasks. When the client build has the
    CSIP coordinator, CAP commander and VCP controller, it also locks and
    releases the server's coordinated set and sets its volume through the
    commander; the server reports the lock changes and the volume it received.
    Returns False (SKIP) when TMAP/GMAP are not compiled into either build.

    A second round then re-runs the whole cycle after audio.end() -> re-create ->
    audio.begin() on both sides without a BLE restart: the server must
    re-register every role and the client must rediscover GMAP (plus
    CSIP/commander when compiled in) through freshly re-linked callbacks.
    """
    mc = client.expect(
        r"\[CLIENT\] TopProfiles (not supported|result tmap=(-?\d+) gmap=(-?\d+) ugt=(-?\d+))", timeout=120
    )
    if b"not supported" in mc.group(0):
        # Neither side runs a reinit round without the roles; the server
        # never waits in this path, so release and collect its lines as usual.
        _release_server(server)
        ms = server.expect(
            r"\[SERVER\] TopProfiles (not supported|result done locks=(\d+) volume=(\d+) reinit=(\d+))", timeout=120
        )
        server.expect_exact("[SERVER] Phase8 top_profiles done", timeout=40)
        return False
    # The round-1 set-control line (guard-dependent) is printed before the
    # client tears down; the quiescence barrier below it gates release #1 so
    # the server never re-begins into a busy GAP. Every client wait loop is
    # bounded, so a missing re-registration fails the reinit expects instead
    # of hanging.
    msc = client.expect(
        r"\[CLIENT\] (SetControl result csip=(-?\d+) locked=(\d+) commander=(\d+)|TopProfiles round1 down=([01]))",
        timeout=60,
    )
    if b"SetControl result" in msc.group(0):
        csip_size, locked, commander = int(msc.group(2)), int(msc.group(3)), int(msc.group(4))
        md = client.expect(r"\[CLIENT\] TopProfiles round1 down=([01])", timeout=60)
        LOGGER.info("top_profiles round1 down=%s", md.group(1).decode())
    _release_server(server)
    msr = server.expect(r"\[SERVER\] TopProfiles reinit ready registered=([01])", timeout=60)
    mc2 = client.expect(r"\[CLIENT\] TopProfiles reinit gmap=(-?\d+) ugt=(-?\d+)", timeout=180)
    gmap_roles2, gmap_ugt2 = int(mc2.group(1)), int(mc2.group(2))
    if b"SetControl result" in msc.group(0):
        msc2 = client.expect(r"\[CLIENT\] TopProfiles reinit csip=(-?\d+)", timeout=40)
        csip_size2 = int(msc2.group(1))
        mcmd2 = client.expect(r"\[CLIENT\] SetControl reinit commander=(\d+)", timeout=40)
        commander2 = int(mcmd2.group(1))
    else:
        csip_size2, commander2 = -1, -1
    client.expect_exact("[CLIENT] Phase8 top_profiles done", timeout=40)
    _release_server(server)
    ms = server.expect(r"\[SERVER\] TopProfiles result done locks=(\d+) volume=(\d+) reinit=(\d+)", timeout=120)
    server.expect_exact("[SERVER] Phase8 top_profiles done", timeout=40)

    if b"SetControl result" in msc.group(0):
        locks, volume = int(ms.group(1)), int(ms.group(2))
        LOGGER.info(
            "set_control: csip=%d locked=%d commander=%d locks=%d volume=%d",
            csip_size,
            locked,
            commander,
            locks,
            volume,
        )
        assert csip_size == 1, f"CSIP coordinator read set size {csip_size}, expected 1"
        assert locked == 1, "CSIP coordinator failed to lock the set"
        assert locks >= 2, f"server saw {locks} lock changes, expected lock + release"
        assert commander == 1, "CAP commander volume procedure failed"
        assert volume == 77, f"server volume is {volume} after the commander set 77"

    tmap_roles = int(mc.group(2))
    gmap_roles = int(mc.group(3))
    gmap_ugt = int(mc.group(4))
    # TMAP CallTerminal=0x02, UnicastMediaReceiver=0x08.
    assert tmap_roles >= 0, "TMAP discovery failed on the client"
    assert (tmap_roles & 0x02) and (tmap_roles & 0x08), f"peer TMAP roles 0x{tmap_roles:02x} missing CT|UMR"
    # GMAP UnicastGameTerminal=0x02; UGT Sink feature=0x04.
    assert gmap_roles >= 0, "GMAP discovery failed on the client"
    assert gmap_roles & 0x02, f"peer GMAP roles 0x{gmap_roles:02x} missing UGT"
    assert gmap_ugt & 0x04, f"peer UGT features 0x{gmap_ugt:02x} missing Sink"
    LOGGER.info("top_profiles: tmap=0x%02x gmap=0x%02x ugt=0x%02x", tmap_roles, gmap_roles, gmap_ugt)
    # Round-2 (reinit) assertions: every callback re-registered after the
    # same-host deinit must work, or discovery observes nothing here.
    assert int(msr.group(1)) == 1, "server failed to re-register roles after audio.end -> audio.begin"
    assert gmap_roles2 >= 0, "GMAP rediscovery failed after client reinit (stale callback link?)"
    assert gmap_roles2 & 0x02, f"peer GMAP roles 0x{gmap_roles2:02x} missing UGT after reinit"
    assert gmap_ugt2 & 0x04, f"peer UGT features 0x{gmap_ugt2:02x} missing Sink after reinit"
    assert int(ms.group(3)) == 1, "server reinit round did not complete"
    if b"SetControl result" in msc.group(0):
        assert csip_size2 == 1, f"CSIP coordinator re-read set size {csip_size2}, expected 1"
        assert commander2 == 1, "CAP commander volume failed after reinit"
    LOGGER.info("top_profiles reinit: gmap=0x%02x ugt=0x%02x", gmap_roles2, gmap_ugt2)
    return True


def _phase_hearing_aid(server, client):
    """Hearing Access Service round-trip (server publishes a preset list, client
    discovers it, reads the presets, and switches the active one).

    The server registers a binaural hearing aid with three presets and counts
    select requests; the client discovers the HAS, reads the records, and sets
    the active preset to index 2. Returns False (SKIP) when the HAS server or
    client is not compiled into a build (only a dual-audio esp32s31 pair with
    HAS + HAS client runs it).

    A second round then re-runs discovery after audio.end() -> re-create ->
    audio.begin() on the client without a BLE restart: the controller callback must be
    re-linked on re-registration, or the preset re-read observes nothing.
    """
    mc = client.expect(
        r"\[CLIENT\] HearingAid (not supported|result disc=(\d+) read=(\d+) switched=(\d+))", timeout=120
    )
    if b"not supported" in mc.group(0):
        _release_server(server)
        ms = server.expect(
            r"\[SERVER\] HearingAid (not supported|result presets=3 selects=(\d+) active=(\d+))", timeout=90
        )
        server.expect_exact("[SERVER] Phase9 hearing_aid done", timeout=40)
        client.expect_exact("[CLIENT] Phase9 hearing_aid done", timeout=40)
        return False
    # Quiescence barrier like Phase 8: release only after the previous link
    # is fully down.
    md = client.expect(r"\[CLIENT\] HearingAid round1 down=([01])", timeout=60)
    LOGGER.info("hearing_aid round1 down=%s", md.group(1).decode())
    _release_server(server)
    mc2 = client.expect(r"\[CLIENT\] HearingAid reinit disc=(\d+) read=(\d+)", timeout=120)
    _release_server(server)
    ms = server.expect(r"\[SERVER\] HearingAid (not supported|result presets=3 selects=(\d+) active=(\d+))", timeout=90)
    server.expect_exact("[SERVER] Phase9 hearing_aid done", timeout=40)
    client.expect_exact("[CLIENT] Phase9 hearing_aid done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    disc = int(mc.group(2))
    read = int(mc.group(3))
    switched = int(mc.group(4))
    selects = int(ms.group(2))
    active = int(ms.group(3))
    assert disc == 1, "client failed to discover the peer HAS"
    assert read >= 3, f"expected >= 3 preset records read, got {read}"
    assert switched == 1, "client preset switch was not accepted"
    assert selects >= 1, "server observed no preset select request"
    assert active == 2, f"server active preset should be 2 after the switch, got {active}"
    disc2, read2 = int(mc2.group(1)), int(mc2.group(2))
    assert disc2 == 1, "client failed to rediscover the peer HAS after reinit"
    assert read2 >= 3, f"expected >= 3 preset records re-read after reinit, got {read2}"
    LOGGER.info("hearing_aid reinit: disc=%d read=%d", disc2, read2)
    return True


def _phase_stereo_broadcast(server, client):
    """Stereo Auracast: 2 BIS (front left + front right) from source to sink.

    The server streams 0x11-filled SDUs on the left BIS and 0x22 on the right;
    the client syncs both BISes and counts SDUs per channel, checking the
    pattern so swapped or merged channels fail. Returns False (SKIP) when the
    broadcast source or sink is not compiled into a build.
    """
    ms = server.expect(r"\[SERVER\] Stereo (not supported|result bis=(\d+) sent=(\d+) ms=(\d+) rate=(\d+))", timeout=90)
    mc = client.expect(
        r"\[CLIENT\] Stereo (not supported|result left=(\d+) right=(\d+) mismatched=(\d+) cb=(\d+) cleared=(\d+))",
        timeout=120,
    )
    server.expect_exact("[SERVER] Phase10 stereo done", timeout=40)
    client.expect_exact("[CLIENT] Phase10 stereo done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    bis, sent, elapsed, rate = int(ms.group(2)), int(ms.group(3)), int(ms.group(4)), int(ms.group(5))
    left, right, mismatched, cb_hits, cleared = (int(mc.group(i)) for i in range(2, 7))
    LOGGER.info(
        "stereo_broadcast: bis=%d sent=%d ms=%d rate=%d/s left=%d right=%d mismatched=%d cb=%d cleared=%d",
        bis,
        sent,
        elapsed,
        rate,
        left,
        right,
        mismatched,
        cb_hits,
        cleared,
    )
    assert bis == 2, f"source created {bis} BIS instead of 2"
    assert left > 0 and right > 0, f"sink did not receive both channels (left={left}, right={right})"
    assert mismatched == 0, f"{mismatched} SDUs arrived on the wrong channel"
    assert elapsed > 0 and rate >= 150, f"broadcast throughput too low: {rate} SDUs/s over {elapsed} ms"
    assert cb_hits > 0, "the churned BIS stream callback was never invoked"
    assert cleared == 1, "clearing callbacks from inside onStarted did not run"
    return True


def _phase_broadcast_assistant(server, client):
    """BAP Broadcast Assistant control path (server = Broadcast Sink + Scan
    Delegator, client = assistant).

    The client discovers the server's BASS, runs Remote Scan Started/Stopped,
    adds a source description (no PA/BIS sync requested, so no third device is
    needed), waits for the receive state that reports it, then removes it and
    waits for the removal notification. Returns False (SKIP) when the scan
    delegator or the assistant is not compiled into a build.
    """
    mc = client.expect(
        r"\[CLIENT\] BroadcastAssistant (not supported|result disc=(\d+) states=(\d+) scan=(\d+) "
        r"added=(\d+) state=(\d+) removed=(\d+) gone=(\d+) stops=(\d+) stopfail=(\d+) again=(\d+))",
        timeout=120,
    )
    _release_server(server)
    ms = server.expect(r"\[SERVER\] BroadcastAssistant (not supported|result delegator=(\d+))", timeout=120)
    server.expect_exact("[SERVER] Phase11 broadcast_assistant done", timeout=40)
    client.expect_exact("[CLIENT] Phase11 broadcast_assistant done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    delegator = int(ms.group(2))
    disc, states, scan, added, state, removed, gone, stops, stopfail, again = (int(mc.group(i)) for i in range(2, 12))
    assert delegator == 1, "server failed to bring up the Broadcast Sink with BASS"
    assert disc == 1, "assistant failed to discover the sink's BASS"
    assert states >= 1, "sink exposes no Broadcast Receive State characteristic"
    assert scan == 1, "Remote Scan Started/Stopped writes were not accepted"
    assert added == 1, "Add Source write was not accepted"
    assert state == 1, "no receive state reported the added source"
    assert removed == 1, "Remove Source write was not accepted"
    assert gone == 1, "the sink did not report the source as removed"
    assert stops == 1 and stopfail == 0, f"stopRemoteScan() was not idempotent (results={stops}, failures={stopfail})"
    assert again == 1, "a repeated stopRemoteScan() did not report success"
    return True


def _phase_cap_handover(server, client):
    """CAP unicast -> broadcast handover (server = CAP acceptor with a unicast
    server and a delegator broadcast sink, client = CAP initiator).

    The client starts a unicast stream, sends raw SDUs, then hands the session
    over to a broadcast and keeps sending on the same stream handle. The server
    counts SDUs on its unicast and broadcast sink streams separately, so audio
    must arrive on both sides of the handover. Returns False (SKIP) when CAP
    handover (client) or the acceptor roles (server) are not compiled in.
    """
    mc = client.expect(
        r"\[CLIENT\] CapHandover (not supported|result disc=(\d+) unicast=(\d+) handover=(\d+) "
        r"ucsent=(\d+) bcsent=(\d+) badho=(\d+) badafter=(\d+) ucstopped=(\d+) ucrc=(-?\d+) bcrc=(-?\d+) "
        r"hofail=(\d+) horestart=(\d+))",
        timeout=150,
    )
    _release_server(server)
    ms = server.expect(
        r"\[SERVER\] CapHandover (not supported|result unicast=(\d+) joined=(\d+) broadcast=(\d+) pba=(\d+))",
        timeout=150,
    )
    server.expect_exact("[SERVER] Phase12 cap_handover done", timeout=40)
    client.expect_exact("[CLIENT] Phase12 cap_handover done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    uc_rx, joined, bc_rx, pba = int(ms.group(2)), int(ms.group(3)), int(ms.group(4)), int(ms.group(5))
    disc, unicast, handover, uc_sent, bc_sent, badho, badafter, uc_stopped, uc_rc, bc_rc, ho_fail, ho_restart = (
        int(mc.group(i)) for i in range(2, 14)
    )
    LOGGER.info("cap_handover: ucsent=%d ucrx=%d bcsent=%d bcrx=%d joined=%d", uc_sent, uc_rx, bc_sent, bc_rx, joined)
    assert disc == 1, "CAP initiator did not discover the acceptor (or it has no sink ASE)"
    assert unicast == 1, "CAP unicast start failed"
    assert uc_rx > 0, f"acceptor received no unicast SDUs (initiator sent {uc_sent})"
    assert handover == 1, "handover to broadcast did not complete"
    assert joined == 1, "acceptor never joined the handed-over BIG"
    assert bc_rx > 0, f"acceptor received no broadcast SDUs after the handover (initiator sent {bc_sent})"
    assert pba == 1, "Public Broadcast Announcement build/parse round-trip failed"
    assert badho == 1, "handoverToBroadcast() accepted an invalid advertising instance"
    assert badafter == 1, "a locally failed handover stopped the running unicast session"
    LOGGER.info(
        "cap_handover: ucstop=%d rc=%d bcstop_rc=%d hofail=%d horestart=%d",
        uc_stopped,
        uc_rc,
        bc_rc,
        ho_fail,
        ho_restart,
    )
    assert (
        bc_rc == 0
    ), f"stopBroadcast() after the handover failed (rc={bc_rc}) — source left live instead of Fail-with-retry"
    assert ho_fail == 1, "the forced asynchronous handover failure did not report"
    assert ho_restart == 1, "the initiator could not rebuild unicast after the forced handover failure"
    return True


def _phase_bcast_stop_race(server, client):
    """Phase 13 — BroadcastSource start()/stop() race (regression).

    The server stops a broadcast source immediately after every start(), five
    times, without waiting for the BIG to come up: stop() must wait out the
    asynchronous start, return OK each round, fire onStopped for every
    terminated BIG, and leave the role able to run one clean streaming session
    afterward (which also proves the advertising instance was released).
    Server-side stress; the client only observes.
    """
    ms = server.expect(
        r"\[SERVER\] StopRace (not supported|result races=(\d+) startOk=(\d+) stopOk=(\d+) "
        r"stoppedEv=(\d+) clean=([01]))",
        timeout=90,
    )
    server.expect_exact("[SERVER] Phase13 bcast_stop_race done", timeout=30)
    client.expect_exact("[CLIENT] Phase13 bcast_stop_race done", timeout=30)
    if b"not supported" in ms.group(0):
        return False
    races, ok_start, ok_stop = int(ms.group(2)), int(ms.group(3)), int(ms.group(4))
    stopped_ev, clean = int(ms.group(5)), int(ms.group(6))
    assert races == 5, f"expected 5 start/stop rounds, got {races}"
    assert ok_start == races, f"{races - ok_start} start() calls failed during the race"
    assert ok_stop == races, f"{races - ok_stop} stop() calls failed during the race"
    assert stopped_ev >= races, f"only {stopped_ev} onStopped events for {races} terminated BIGs"
    assert clean == 1, "the source could not run a clean streaming session after the race"
    return True


def _phase_cap_bc_interleave(server, client):
    """Phase 14 — CAP startBroadcast()/stopBroadcast() vs concurrent stream() (regression).

    The server interleaves five start/stop rounds of a CAP initiator broadcast
    while a separate task hammers streamCount()/stream(). Regression for the
    by-value streams() snapshot and the atomic slot fields: no torn handle,
    no stuck state, and one clean broadcast still runs afterward.
    Server-side stress; the client only observes.
    """
    ms = server.expect(
        r"\[SERVER\] CapBC (not supported|result rounds=(\d+) startOk=(\d+) stopOk=(\d+) "
        r"stoppedEv=(\d+) reads=(\d+) clean=([01]))",
        timeout=90,
    )
    server.expect_exact("[SERVER] Phase14 cap_bc_interleave done", timeout=30)
    client.expect_exact("[CLIENT] Phase14 cap_bc_interleave done", timeout=30)
    if b"not supported" in ms.group(0):
        return False
    rounds, ok_start, ok_stop = int(ms.group(2)), int(ms.group(3)), int(ms.group(4))
    stopped_ev, reads, clean = int(ms.group(5)), int(ms.group(6)), int(ms.group(7))
    assert rounds == 5, f"expected 5 start/stop rounds, got {rounds}"
    assert ok_start == rounds, f"{rounds - ok_start} startBroadcast() calls failed during the interleave"
    assert ok_stop == rounds, f"{rounds - ok_stop} stopBroadcast() calls failed during the interleave"
    assert stopped_ev >= rounds, f"only {stopped_ev} onBroadcastStopped events for {rounds} stopped broadcasts"
    assert reads > 0, "the concurrent snapshot reader never ran"
    assert clean == 1, "the initiator could not run a clean broadcast after the interleave"
    return True


def _phase_start_retry(server, client):
    """Phase 15 — start() refuses a retry after a mid-start failure (regression).

    The server exhausts the broadcast-sink stream pool so a sink apply()
    fails: the first start() must fail, an immediate second start() must
    return InvalidState (not run half-registered roles again), and
    end()/begin() must recover to a working start(). Also covers the PACS
    boot-pinned re-begin (different contexts still start) and the over-long
    TBS provider rejection, and the end() contract (Busy from a stack
    callback, safe double-end with begin() recovery).
    """
    ms = server.expect(
        r"\[SERVER\] StartRetry (not supported|first=(-?\d+) retry=(-?\d+) recovered=(\d+) pacs=(-?\d)(-?\d) "
        r"tbslong=(-?\d+) endtwice=(-?\d+) endfromcb=(-?\d+) endrebegin=(-?\d+))",
        timeout=180,
    )
    server.expect_exact("[SERVER] Phase15 start_retry_refused done", timeout=30)
    client.expect_exact("[CLIENT] Phase15 start_retry_refused done", timeout=30)
    if b"not supported" in ms.group(0):
        return False
    first, retry, recovered = int(ms.group(2)), int(ms.group(3)), int(ms.group(4))
    pacs_first, pacs_second, tbs_long = int(ms.group(5)), int(ms.group(6)), int(ms.group(7))
    if first == 0:
        return False  # pool bigger than the probe; nothing to refuse
    assert first != -1, "audio.begin() failed during the pool-exhaustion setup"
    assert first != 0, "pool exhaustion did not fail the first start()"
    assert retry == -4, f"retry after a failed start() did not return InvalidState (rc={retry})"
    assert recovered == 1, "end()/begin() did not recover to a working start()"
    if pacs_first >= 0:
        assert pacs_first == 1, "PACS first begin/start failed"
        assert pacs_second == 1, "PACS re-begin with different contexts failed (boot-pinned path)"
    if tbs_long >= 0:
        assert tbs_long == 1, "over-long TBS provider name did not fail start()"
    end_twice, end_from_cb, end_rebegin = int(ms.group(8)), int(ms.group(9)), int(ms.group(10))
    if end_twice >= 0:
        assert end_from_cb == -12, f"end() from a stack callback did not refuse with Busy (rc={end_from_cb})"
        assert end_twice == 1, "double end() did not leave a clean state"
        assert end_rebegin == 1, "lazy init after double end() did not recover (factory + start without begin)"
    return True


def _phase_security_policy(server, client):
    """Phase 16 — the audio security policy is explicit and opt-out.

    Default on, setAutoSecurityPolicy(false) sticks, and begin() still
    succeeds unconfigured (with a warning instead of silent policy).
    """
    ms = server.expect(r"\[SERVER\] SecPolicy (not supported|default=(\d+) setOff=(\d+) beginOk=(\d+))", timeout=60)
    server.expect_exact("[SERVER] Phase16 security_policy done", timeout=30)
    client.expect_exact("[CLIENT] Phase16 security_policy done", timeout=30)
    if b"not supported" in ms.group(0):
        return False
    assert int(ms.group(2)) == 1, "autoSecurityPolicy() default is not true"
    assert int(ms.group(3)) == 1, "setAutoSecurityPolicy(false) did not stick"
    assert int(ms.group(4)) == 1, "begin() with the policy off did not succeed"
    return True


def _phase_iso_gates(server, client):
    """Phase 17 — connectCis() refuses dead handles before any CIG exists.

    0xFFFF and a never-connected handle must both return nullptr (fail
    closed, no HCI), proving the liveness gate added for the stale-ACL fix.
    """
    ms = server.expect(r"\[SERVER\] IsoGates (not supported|nullFFFF=(\d+) nullDead=(\d+))", timeout=60)
    server.expect_exact("[SERVER] Phase17 iso_handle_gates done", timeout=30)
    client.expect_exact("[CLIENT] Phase17 iso_handle_gates done", timeout=30)
    if b"not supported" in ms.group(0):
        return False
    assert int(ms.group(2)) == 1, "connectCis(0xFFFF) did not return nullptr"
    assert int(ms.group(3)) == 1, "connectCis() on a dead handle did not return nullptr"
    return True


def _phase_mic_disable(server, client):
    """Phase 18 — MICS mute tri-state and fast unmute refusal.

    The client mutes, the server disables mute, and the client must observe
    unmuted + disabled (not a mute change), have unmute() fail fast with
    InvalidState, and see no asynchronous onResult for the refused write.
    """
    mc = client.expect(
        r"\[CLIENT\] MicDisable (not supported|disabled=(\d+) unmuteRc=(-?\d+) noresult=(\d+))",
        timeout=120,
    )
    ms = server.expect(r"\[SERVER\] MicDisable (not supported|disabled=(\d+))", timeout=60)
    server.expect_exact("[SERVER] Phase18 mic_mute_disabled done", timeout=30)
    client.expect_exact("[CLIENT] Phase18 mic_mute_disabled done", timeout=30)
    if b"not supported" in mc.group(0) or b"not supported" in ms.group(0):
        return False
    assert int(ms.group(2)) == 1, "server did not report mute disabled"
    assert int(mc.group(2)) == 1, "client did not observe mute disabled on discovery"
    assert int(mc.group(3)) == -4, f"unmute() while disabled did not fail fast with InvalidState (rc={mc.group(3)})"
    assert int(mc.group(4)) == 1, "a refused unmute fired onResult"
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


def test_ble_audio(dut, ci_job_id, record_property):
    server = dut[0]
    client = dut[1]

    name = "BLEAU_" + (ci_job_id if ci_job_id else rand_str4())
    LOGGER.info("LE Audio combined test name: %s", name)

    server.expect_exact("[SERVER] Device ready for name", timeout=120)
    client.expect_exact("[CLIENT] Device ready for name", timeout=120)
    server.expect_exact("[SERVER] Send name:")
    client.expect_exact("[CLIENT] Send name:")
    server.write(name)
    client.write(name)
    server.expect_exact(f"[SERVER] Name: {name}", timeout=10)
    client.expect_exact(f"[CLIENT] Target: {name}", timeout=10)
    sr = server.expect(r"\[SERVER\] Audio validation required: ([01])", timeout=10)
    cr = client.expect(r"\[CLIENT\] Audio validation required: ([01])", timeout=10)
    assert sr.group(1) == cr.group(1), "server/client target requirements disagree"
    audio_required = sr.group(1) == b"1"

    # (phase_fn, *args); all audio phases self-skip on unsupported builds.
    PHASES = {
        1: (_phase_audio_lifecycle, server, client),  # skip if falsy
        2: (_phase_iso_cis, server, client),  # skip if falsy
        3: (_phase_iso_bis, server, client),  # skip if falsy
        4: (_phase_bap_unicast, server, client),  # skip if falsy
        5: (_phase_bap_broadcast, server, client),  # skip if falsy
        6: (_phase_lc3_loopback, server, client),  # skip if falsy
        7: (_phase_control_profiles, server, client),  # skip if falsy
        8: (_phase_top_profiles, server, client),  # skip if falsy
        9: (_phase_hearing_aid, server, client),  # skip if falsy
        10: (_phase_stereo_broadcast, server, client),  # skip if falsy
        11: (_phase_broadcast_assistant, server, client),  # skip if falsy
        12: (_phase_cap_handover, server, client),  # skip if falsy
        13: (_phase_bcast_stop_race, server, client),
        14: (_phase_cap_bc_interleave, server, client),
        15: (_phase_start_retry, server, client),
        16: (_phase_security_policy, server, client),
        17: (_phase_iso_gates, server, client),
        18: (_phase_mic_disable, server, client),
        19: (_phase_memory_release, server, client),
    }

    SKIP_REASONS = {
        1: "LE Audio not supported",
        2: "host ISO not supported (needs dual esp32s31)",
        3: "host ISO not supported (needs dual esp32s31)",
        4: "LE Audio engine not supported (needs dual esp32s31)",
        5: "LE Audio engine not supported (needs dual esp32s31)",
        6: "LC3 codec not supported (needs esp_audio_codec on dual esp32s31)",
        7: "LE Audio engine not supported (needs dual esp32s31)",
        8: "TMAP/GMAP not supported (needs dual esp32s31)",
        9: "HAS server/client not supported (needs dual esp32s31)",
        10: "broadcast source/sink not supported (needs dual esp32s31)",
        11: "scan delegator / broadcast assistant not supported (needs dual esp32s31)",
        12: "CAP handover not supported (needs CONFIG_BT_CAP_HANDOVER on dual esp32s31)",
        13: "broadcast source not supported (needs dual esp32s31)",
        14: "CAP initiator not supported (needs dual esp32s31)",
        15: "broadcast sink not supported (needs dual esp32s31)",
        16: "LE Audio engine not supported (needs dual esp32s31)",
        17: "host ISO not supported (needs dual esp32s31)",
        18: "MICP device/controller not supported (needs dual esp32s31)",
    }

    passed, failed = [], []

    for num, (fn, *args) in PHASES.items():
        label = PHASE_LABELS[num]
        LOGGER.info("Running phase %d: %s", num, label)
        _start_phase(server, client, num)
        try:
            result = fn(*args)
            if num in SKIP_REASONS and not result:
                assert (
                    not audio_required
                ), f"esp32s31 validation must execute phase {num} ({label}); skip-green is forbidden"
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

    summary = f"{len(passed)} passed, {len(failed)} failed out of {len(PHASE_LABELS)}"
    LOGGER.info("Summary: %s", summary)
    record_property("summary", summary)

    if failed:
        lines = [f"  {lbl}: {err}" for lbl, err in failed]
        pytest.fail(f"{len(failed)} phase(s) failed:\n" + "\n".join(lines))
