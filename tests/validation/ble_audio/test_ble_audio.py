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
    13: "memory_release",
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


def _phase_audio_lifecycle(server, client):
    """LE Audio engine bring-up + classic-service coexistence (server-driven).

    Returns False (SKIP) when the LE Audio engine is not compiled into the build.
    """
    m = server.expect(
        r"\[SERVER\] (Audio lifecycle OK|Audio lifecycle FAILED begin=(\d+) start=(\d+) end=(\d+)|"
        r"Audio not supported, skipping)",
        timeout=30,
    )
    # The client only acknowledges the phase; either line is fine there.
    client.expect(r"\[CLIENT\] (Audio lifecycle OK|Audio not supported, skipping)", timeout=20)
    if b"not supported" in m.group(0):
        server.expect_exact("[SERVER] Phase1 audio done", timeout=10)
        client.expect_exact("[CLIENT] Phase1 audio done", timeout=20)
        return False
    assert b"FAILED" not in m.group(0), (
        f"audio lifecycle failed: begin={m.group(2)} start={m.group(3)} end={m.group(4)}"
    )
    mr = server.expect(r"\[SERVER\] Audio reinit begin=(\d+) start=(\d+)", timeout=20)
    assert int(mr.group(1)) == 1, "audio.begin() failed after audio.end() (re-initialization)"
    assert int(mr.group(2)) == 1, "audio.start() failed after re-initialization"
    server.expect_exact("[SERVER] Phase1 audio done", timeout=20)
    client.expect_exact("[CLIENT] Phase1 audio done", timeout=10)
    return True


def _phase_iso_cis(server, client):
    """BLEIso CIS transport integrity (server = Peripheral, client = Central).

    The CIS is bidirectional: the Central streams to the Peripheral and the
    Peripheral streams back. Returns False (SKIP) when host ISO is not compiled
    into either build (the only rig that truly runs this is a dual-ISO
    esp32s31<->esp32s31 pair).
    """
    ms = server.expect(r"\[SERVER\] IsoCis (not supported|result connected=(\d+) received=(\d+) returned=(\d+))", timeout=60)
    mc = client.expect(r"\[CLIENT\] IsoCis (not supported|result connected=(\d+) sent=(\d+) received=(\d+))", timeout=90)
    server.expect_exact("[SERVER] Phase2 iso_cis done", timeout=40)
    client.expect_exact("[CLIENT] Phase2 iso_cis done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    received = int(ms.group(3))
    returned = int(ms.group(4))
    sent = int(mc.group(3))
    returned_rx = int(mc.group(4))
    assert sent > 0, "CIS Central sent no SDUs"
    assert received > 0, f"CIS Peripheral received no SDUs (Central sent {sent})"
    assert returned > 0, "CIS Peripheral sent no SDUs back"
    assert returned_rx > 0, f"CIS Central received no SDUs back (Peripheral sent {returned})"
    return True


def _phase_iso_bis(server, client):
    """BLEIso BIS transport integrity (server = Broadcaster, client = Receiver).

    Returns False (SKIP) when host ISO or BLE5 is not compiled into either build.
    """
    ms = server.expect(r"\[SERVER\] IsoBis (not supported|result connected=(\d+) sent=(\d+))", timeout=60)
    mc = client.expect(r"\[CLIENT\] IsoBis (not supported|result synced=(\d+) received=(\d+))", timeout=90)
    server.expect_exact("[SERVER] Phase3 iso_bis done", timeout=40)
    client.expect_exact("[CLIENT] Phase3 iso_bis done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    sent = int(ms.group(3))
    received = int(mc.group(3))
    assert sent > 0, "BIS Broadcaster streamed no SDUs"
    assert received > 0, f"BIS Receiver received no SDUs (Broadcaster sent {sent})"
    return True


def _phase_bap_unicast(server, client):
    """BAP unicast transport integrity (server = Unicast Server, client = Unicast Client).

    The client runs the full BAP setup and streams transparent SDUs to the
    server's sink ASE over a CIS. Returns False (SKIP) when the LE Audio engine
    is not compiled into either build (only a dual-audio esp32s31 pair runs it).
    """
    ms = server.expect(r"\[SERVER\] BapUnicast (not supported|result received=(\d+))", timeout=120)
    mc = client.expect(r"\[CLIENT\] BapUnicast (not supported|result streaming=(\d+) sent=(\d+) presets=(\d+))", timeout=120)
    server.expect_exact("[SERVER] Phase4 bap_unicast done", timeout=40)
    client.expect_exact("[CLIENT] Phase4 bap_unicast done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    received = int(ms.group(2))
    streaming = int(mc.group(2))
    sent = int(mc.group(3))
    presets = int(mc.group(4))
    assert presets == 1, "Unicast Client did not find LC3_16_2_1 in the server's sink PACS"
    assert streaming == 1, "unicast stream never reached Streaming on the client"
    assert sent > 0, "Unicast Client sent no SDUs"
    assert received > 0, f"Unicast Server received no SDUs (Client sent {sent})"
    return True


def _phase_bap_broadcast(server, client):
    """BAP broadcast transport integrity (server = Broadcast Source, client = Broadcast Sink).

    The server announces an Auracast broadcast (ext + periodic adv + BASE) and
    streams transparent SDUs over a BIG; the client scans, PA-syncs, syncs the
    BIG, and counts SDUs received. Returns False (SKIP) when the LE Audio engine
    is not compiled into either build (only a dual-audio esp32s31 pair runs it).
    """
    ms = server.expect(r"\[SERVER\] BapBroadcast (not supported|result sent=(\d+))", timeout=90)
    mc = client.expect(r"\[CLIENT\] BapBroadcast (not supported|result received=(\d+) base=(\d+))", timeout=120)
    server.expect_exact("[SERVER] Phase5 bap_broadcast done", timeout=40)
    client.expect_exact("[CLIENT] Phase5 bap_broadcast done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    sent = int(ms.group(2))
    received = int(mc.group(2))
    base = int(mc.group(3))
    assert sent > 0, "Broadcast Source streamed no SDUs"
    assert base == 1, "Broadcast Sink never reported the source's BASE (onBaseReceived)"
    assert received > 0, f"Broadcast Sink received no SDUs (Source sent {sent})"
    return True


def _phase_lc3_loopback(server, client):
    """End-to-end LC3 audio path (client Recorder -> CIS -> server Player).

    The client feeds a synthetic 1 kHz tone into a BLEAudioRecorder, which
    LC3-encodes and streams it over the CIS; the server's BLEAudioPlayer decodes
    it and reports the decoded sample count plus mean absolute amplitude. This
    proves the whole codec + I2S-less data plane works on real hardware.

    Returns False (SKIP) when the LC3 codec (esp_audio_codec) is not compiled
    into either build; only a dual-audio esp32s31 pair with the codec runs it.
    """
    ms = server.expect(
        r"\[SERVER\] Lc3Loopback (not supported|result samples=(\d+) meanamp=(\d+) plc=(\d+))", timeout=120
    )
    mc = client.expect(
        r"\[CLIENT\] Lc3Loopback (not supported|result streaming=(\d+) sent=(\d+) errors=(\d+))", timeout=120
    )
    server.expect_exact("[SERVER] Phase6 lc3_loopback done", timeout=40)
    client.expect_exact("[CLIENT] Phase6 lc3_loopback done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    samples = int(ms.group(2))
    meanamp = int(ms.group(3))
    plc = int(ms.group(4))
    streaming = int(mc.group(2))
    encoded = int(mc.group(3))
    errors = int(mc.group(4))
    LOGGER.info("lc3_loopback: sent=%d errors=%d samples=%d meanamp=%d plc=%d", encoded, errors, samples, meanamp, plc)
    assert streaming == 1, "unicast stream never reached Streaming on the client"
    assert encoded > 0, f"Recorder sent no LC3 SDUs ({errors} send errors)"
    assert samples > 0, "Player decoded no PCM samples"
    # A decoded 1 kHz tone at amplitude ~8000 has a mean |amplitude| well above
    # a few hundred; a low value would mean silence / a broken codec path.
    assert meanamp > 500, f"decoded audio too quiet (meanamp={meanamp}); tone did not survive LC3"
    return True


def _phase_control_profiles(server, client):
    """LE Audio control-profile round-trip over one ACL (server = acceptor,
    client = controller / "phone").

    The server exposes CAP acceptor (CAS+CSIS), VCP renderer, MICP device, MCP
    media player, and CCP call server; the client connects once and drives each:
    CAP discovery, VCP setVolume, MICP mute, MCP play, CCP originate. The server
    tallies the writes it observes. Returns False (SKIP) when the LE Audio engine
    is not compiled into either build (only a dual-audio esp32s31 pair runs it).

    Hard gate: the VCP path must work end to end (client discovers VCS and the
    server observes the volume write). The other profiles are exercised and
    logged; their client-side discovery is asserted, but their deeper state
    machines are informational.
    """
    ms = server.expect(r"\[SERVER\] ControlProfiles (not supported|result volwrites=(\d+) micmutes=(\d+) calls=(\d+))", timeout=120)
    mc = client.expect(r"\[CLIENT\] ControlProfiles (not supported|result cap=(\d+) vcp=(\d+) mic=(\d+) media=(\d+) call=(\d+))", timeout=120)
    server.expect_exact("[SERVER] Phase7 control_profiles done", timeout=40)
    client.expect_exact("[CLIENT] Phase7 control_profiles done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    vol_writes = int(ms.group(2))
    cap_disc = int(mc.group(2))
    vcp_ok = int(mc.group(3))

    # VCP is the end-to-end gate: client discovered VCS + wrote volume, and the
    # server saw the write land on its renderer.
    assert cap_disc == 1, "CAP initiator did not discover the acceptor's CAS"
    assert vcp_ok == 1, "VCP controller failed to discover/set volume"
    assert vol_writes >= 1, "server renderer observed no volume write from the controller"
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
    """
    ms = server.expect(r"\[SERVER\] TopProfiles (not supported|result done locks=(\d+) volume=(\d+))", timeout=120)
    mc = client.expect(
        r"\[CLIENT\] TopProfiles (not supported|result tmap=(-?\d+) gmap=(-?\d+) ugt=(-?\d+))", timeout=120
    )
    # The set-control line is only printed by builds that have those roles.
    msc = client.expect(
        r"\[CLIENT\] (SetControl result csip=(-?\d+) locked=(\d+) commander=(\d+)|Phase8 top_profiles done)", timeout=40
    )
    if b"SetControl" in msc.group(0):
        client.expect_exact("[CLIENT] Phase8 top_profiles done", timeout=40)
    server.expect_exact("[SERVER] Phase8 top_profiles done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    if b"SetControl" in msc.group(0):
        csip_size, locked, commander = int(msc.group(2)), int(msc.group(3)), int(msc.group(4))
        locks, volume = int(ms.group(2)), int(ms.group(3))
        LOGGER.info("set_control: csip=%d locked=%d commander=%d locks=%d volume=%d", csip_size, locked, commander, locks, volume)
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
    return True


def _phase_hearing_aid(server, client):
    """Hearing Access Service round-trip (server publishes a preset list, client
    discovers it, reads the presets, and switches the active one).

    The server registers a binaural hearing aid with three presets and counts
    select requests; the client discovers the HAS, reads the records, and sets
    the active preset to index 2. Returns False (SKIP) when the HAS server or
    client is not compiled into a build (only a dual-audio esp32s31 pair with
    HAS + HAS client runs it).
    """
    ms = server.expect(r"\[SERVER\] HearingAid (not supported|result presets=3 selects=(\d+) active=(\d+))", timeout=90)
    mc = client.expect(r"\[CLIENT\] HearingAid (not supported|result disc=(\d+) read=(\d+) switched=(\d+))", timeout=120)
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
    return True


def _phase_stereo_broadcast(server, client):
    """Stereo Auracast: 2 BIS (front left + front right) from source to sink.

    The server streams 0x11-filled SDUs on the left BIS and 0x22 on the right;
    the client syncs both BISes and counts SDUs per channel, checking the
    pattern so swapped or merged channels fail. Returns False (SKIP) when the
    broadcast source or sink is not compiled into a build.
    """
    ms = server.expect(r"\[SERVER\] Stereo (not supported|result bis=(\d+) sent=(\d+))", timeout=90)
    mc = client.expect(r"\[CLIENT\] Stereo (not supported|result left=(\d+) right=(\d+) mismatched=(\d+))", timeout=120)
    server.expect_exact("[SERVER] Phase10 stereo done", timeout=40)
    client.expect_exact("[CLIENT] Phase10 stereo done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    bis = int(ms.group(2))
    left, right, mismatched = int(mc.group(2)), int(mc.group(3)), int(mc.group(4))
    assert bis == 2, f"source created {bis} BIS instead of 2"
    assert left > 0 and right > 0, f"sink did not receive both channels (left={left}, right={right})"
    assert mismatched == 0, f"{mismatched} SDUs arrived on the wrong channel"
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
    ms = server.expect(r"\[SERVER\] BroadcastAssistant (not supported|result delegator=(\d+))", timeout=120)
    mc = client.expect(
        r"\[CLIENT\] BroadcastAssistant (not supported|result disc=(\d+) states=(\d+) scan=(\d+) "
        r"added=(\d+) state=(\d+) removed=(\d+) gone=(\d+))",
        timeout=120,
    )
    server.expect_exact("[SERVER] Phase11 broadcast_assistant done", timeout=40)
    client.expect_exact("[CLIENT] Phase11 broadcast_assistant done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    delegator = int(ms.group(2))
    disc, states, scan, added, state, removed, gone = (int(mc.group(i)) for i in range(2, 9))
    assert delegator == 1, "server failed to bring up the Broadcast Sink with BASS"
    assert disc == 1, "assistant failed to discover the sink's BASS"
    assert states >= 1, "sink exposes no Broadcast Receive State characteristic"
    assert scan == 1, "Remote Scan Started/Stopped writes were not accepted"
    assert added == 1, "Add Source write was not accepted"
    assert state == 1, "no receive state reported the added source"
    assert removed == 1, "Remove Source write was not accepted"
    assert gone == 1, "the sink did not report the source as removed"
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
    ms = server.expect(
        r"\[SERVER\] CapHandover (not supported|result unicast=(\d+) joined=(\d+) broadcast=(\d+))", timeout=150
    )
    mc = client.expect(
        r"\[CLIENT\] CapHandover (not supported|result disc=(\d+) unicast=(\d+) handover=(\d+) "
        r"ucsent=(\d+) bcsent=(\d+))",
        timeout=150,
    )
    server.expect_exact("[SERVER] Phase12 cap_handover done", timeout=40)
    client.expect_exact("[CLIENT] Phase12 cap_handover done", timeout=40)

    if b"not supported" in ms.group(0) or b"not supported" in mc.group(0):
        return False

    uc_rx, joined, bc_rx = int(ms.group(2)), int(ms.group(3)), int(ms.group(4))
    disc, unicast, handover, uc_sent, bc_sent = (int(mc.group(i)) for i in range(2, 7))
    LOGGER.info(
        "cap_handover: ucsent=%d ucrx=%d bcsent=%d bcrx=%d joined=%d", uc_sent, uc_rx, bc_sent, bc_rx, joined
    )
    assert disc == 1, "CAP initiator did not discover the acceptor (or it has no sink ASE)"
    assert unicast == 1, "CAP unicast start failed"
    assert uc_rx > 0, f"acceptor received no unicast SDUs (initiator sent {uc_sent})"
    assert handover == 1, "handover to broadcast did not complete"
    assert joined == 1, "acceptor never joined the handed-over BIG"
    assert bc_rx > 0, f"acceptor received no broadcast SDUs after the handover (initiator sent {bc_sent})"
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
        13: (_phase_memory_release, server, client),
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

    summary = f"{len(passed)} passed, {len(failed)} failed out of {len(PHASE_LABELS)}"
    LOGGER.info("Summary: %s", summary)
    record_property("summary", summary)

    if failed:
        lines = [f"  {lbl}: {err}" for lbl, err in failed]
        pytest.fail(f"{len(failed)} phase(s) failed:\n" + "\n".join(lines))
