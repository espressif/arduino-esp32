"""
LE Audio host-interop validation (single DUT + Linux host driving Bumble on an
LE-Audio-capable USB dongle).

The ESP is the LE Audio peripheral/acceptor and Auracast source; this pytest
process runs on the Linux bench and drives the local adapter via Bumble
(exclusive HCI — BlueZ must be stopped/masked; see setup_bumble_hci.sh):

- Control plane (phase control_gatt): Bumble GATT client connects once and
  validates every profile the DUT exposes — topology, coexistence, VCP, MICP,
  MCP, CCP, CSIP, CAP, HAS, TMAP, GMAP — cross-checking the DUT's serial
  "[DUT] ..." event lines where the DUT observes a control write. BASS has its
  own phase (scan_delegator).
- Auracast discovery (phase broadcast_announcement): scans extended advertising
  for the Broadcast Audio Announcement (0x1852) + Broadcast ID.
- Audio data plane (phases unicast_audio / broadcast_audio): Bumble ASCS/CIS
  (unicast client) or PA/BIG sync (broadcast sink) + LC3 encode/decode; the DUT
  decodes what we send (sample count + amplitude over serial) and we analyze
  what it streams (a 1 kHz tone) with a Goertzel filter.
- Phone-assisted Auracast (BLE_AUDIO_PHONE, default on): when the Linux HCI
  cannot BIG-sync, a Galaxy S23/S24 is the broadcast *sink* in phase 4 (Listen)
  and the broadcast *source* in phase 5 (Broadcast sound using Auracast).

This is a bench/HITL suite: it is tagged ``le_audio_host`` (no normal-CI runner
provides an LE Audio dongle) and self-skips, per phase, when the required
tooling (bumble/lc3, a free HCI controller) is not present.
"""

import asyncio
import logging
import os
import select
import sys
import time
from pathlib import Path

import pytest

from conftest import rand_str4
import le_audio_host_lib as le

LOGGER = logging.getLogger(__name__)

# TEMPORARY — keep in sync with ble_audio_host.ino::BLE_AUDIO_HOST_PHASE_SOFT_REBOOT.
# audio.end() releases every profile through common_deinit, so audio.begin()
# may run again in one boot; the suite still runs one phase per boot until
# same-boot re-init passes on this bench. Then set both to False/0 and delete
# the soft-reboot branches.
PHASE_SOFT_REBOOT = True

# Phone-assisted Auracast (Galaxy S23/S24 One UI 6.1+). Default on for this HITL
# bench: BE200 has CIS but no Synchronized Receiver / Isochronous Broadcaster.
# Set BLE_AUDIO_PHONE=0 to keep the old skip when Bumble cannot BIG-sync.
# Confirm a listen-side check by pressing Enter or `touch /tmp/bleah_phone_ok`.
PHONE_OK_MARKER = Path(os.environ.get("BLE_AUDIO_PHONE_OK", "/tmp/bleah_phone_ok"))

PHASE_LABELS = {
    1: "control_gatt",
    2: "broadcast_announcement",
    3: "unicast_audio",
    4: "broadcast_audio",
    5: "scan_delegator",
    6: "memory_release",
}

BROADCAST_ID = 0x123456


class PhaseSkip(Exception):
    """Raised by a phase to record a SKIP (feature/tooling unavailable)."""


def _handshake_name(dut, name, timeout=120):
    dut.expect_exact("[DUT] Device ready for name", timeout=timeout)
    dut.expect_exact("[DUT] Send name:")
    dut.write(f"{name}\n")
    dut.expect_exact(f"[DUT] Name: {name}", timeout=10)


def _advance(dut, n):
    """Consume "[DUT] Phase <n> started" and the done marker of phase n-1.

    The done marker follows the start line when phase n-1 ran until the host
    advanced it, and precedes it when phase n-1 returned early, so whichever
    comes first is matched before waiting for the other. Returns False when
    the done marker never shows up (a teardown crash or hang), so the caller
    can report the previous phase instead of silently moving on.
    """
    done = rf"Phase{n - 1} {PHASE_LABELS[n - 1]} done"
    m = dut.expect(rf"\[DUT\] (Phase {n} started|{done})", timeout=15)
    if b"done" in m.group(0):
        dut.expect_exact(f"[DUT] Phase {n} started", timeout=15)
        return True
    try:
        dut.expect(rf"\[DUT\] {done}", timeout=20)
        return True
    except Exception:  # noqa: BLE001 — pexpect timeout / EOF
        LOGGER.error("phase %d (%s): no done marker after teardown", n - 1, PHASE_LABELS[n - 1])
        return False


def _start_phase(dut, n, name=None):
    """Advance the DUT into phase n.

    With PHASE_SOFT_REBOOT: phase 1 is a cold start; phases 2+ first send
    START_PHASE_n to end the previous phase (DUT then ESP.restart()s), then
    re-handshake the name and START_PHASE_n on the fresh boot. Without the
    flag, a single START_PHASE_n is enough (same-boot end/begin).

    Returns whether the previous phase's done marker arrived (True for phase 1).
    """
    LOGGER.info("START_PHASE_%d", n)
    # Let the DUT drain the previous phase's disconnect before it tears down
    # (avoids a controller-deinit race with a live ACL).
    time.sleep(2)
    dut.write(f"START_PHASE_{n}\n")

    if n == 1:
        dut.expect_exact(f"[DUT] Phase {n} started", timeout=10)
        return True

    if not PHASE_SOFT_REBOOT:
        return _advance(dut, n)

    assert name, "name required to re-handshake after soft-reboot"
    # First "Phase n started" is printed while still on the previous boot
    # (advances currentPhase so the body exits and ESP.restart()s). A panic
    # during teardown still reboots, so the handshake below works either way;
    # the missing done marker is what reports it.
    teardown_ok = _advance(dut, n)
    _handshake_name(dut, name, timeout=90)
    dut.write(f"START_PHASE_{n}\n")
    dut.expect_exact(f"[DUT] Phase {n} started", timeout=10)
    return teardown_ok


def _run(coro):
    return asyncio.run(coro)


def _phone_assist():
    v = os.environ.get("BLE_AUDIO_PHONE", "1").strip().lower()
    return v not in ("0", "false", "no", "off")


def _wait_phone_ok(prompt: str, timeout: float = 180.0) -> bool:
    """Wait for the operator to confirm a phone-side Auracast step."""
    try:
        PHONE_OK_MARKER.unlink()
    except FileNotFoundError:
        pass
    LOGGER.warning("%s", prompt)
    print("\n" + prompt + f"\n  Then press Enter here, or: touch {PHONE_OK_MARKER}\n", flush=True)
    deadline = time.time() + timeout
    while time.time() < deadline:
        if PHONE_OK_MARKER.exists():
            LOGGER.info("phone confirm via %s", PHONE_OK_MARKER)
            return True
        if sys.stdin.isatty():
            r, _, _ = select.select([sys.stdin], [], [], 0.25)
            if r:
                sys.stdin.readline()
                LOGGER.info("phone confirm via Enter")
                return True
        else:
            time.sleep(0.25)
    return False


def _require_bumble():
    if not le.bumble_available():
        raise PhaseSkip("bumble[auracast] / lc3 not installed")


# ---------------------------------------------------------------------------
# Phase 1 — control plane (Bumble GATT + serial cross-checks)
# ---------------------------------------------------------------------------


async def _control_gatt(dut, name, record, local_svcs):
    _require_bumble()
    LOGGER.info("control_gatt: opening Bumble session (%s)", le.default_transport())
    async with le.bumble_session() as device:
        address = await le.scan_by_name(device, name, timeout=20.0)
        assert address, f"DUT '{name}' not found in scan"
        LOGGER.info("Connecting to %s", address)
        gatt = await le.Gatt.connect(device, address, timeout=25.0, name=name)

        paired = await gatt.pair()
        encrypted = bool(getattr(gatt.connection, "is_encrypted", paired))
        LOGGER.info("pair() -> %s, encrypted=%s", paired, encrypted)
        record("control_gatt.paired", "PASS" if encrypted else "WARN: link not encrypted")

        LOGGER.info("Discovered services: %s", sorted(gatt.service_uuids()))
        LOGGER.info("Attribute layout:\n%s", gatt.layout())

        errors = []

        def check(label, fn):
            try:
                fn()
                record(f"control_gatt.{label}", "PASS")
                LOGGER.info("control_gatt.%s PASS", label)
            except Exception as e:  # noqa: BLE001
                record(f"control_gatt.{label}", f"FAIL: {e}")
                LOGGER.error("control_gatt.%s FAIL: %s", label, e)
                errors.append((label, str(e)))

        async def acheck(label, coro):
            try:
                await coro
                record(f"control_gatt.{label}", "PASS")
                LOGGER.info("control_gatt.%s PASS", label)
            except Exception as e:  # noqa: BLE001
                record(f"control_gatt.{label}", f"FAIL: {e}")
                LOGGER.error("control_gatt.%s FAIL: %s", label, e)
                errors.append((label, str(e)))

        async def probe(label, coro):
            try:
                res = await coro
                LOGGER.info(
                    "  probe %-28s OK %s",
                    label,
                    res.hex() if isinstance(res, (bytes, bytearray)) else "",
                )
            except Exception as e:  # noqa: BLE001
                LOGGER.warning("  probe %-28s FAIL %s", label, e)

        try:
            # BASS is intentionally absent here (scan_delegator phase).
            # CAS is checked DUT-side (no characteristics; may not enumerate).
            def _topology():
                mandatory = ["PACS", "ASCS", "VCS", "MICS", "CSIS", "HAS", "TMAS"]
                missing = [n for n in mandatory if not gatt.has_service(le.SVC[n])]
                assert not missing, f"missing services: {missing}"
                assert gatt.has_service(le.SVC["MCS"]) or gatt.has_service(le.SVC["GMCS"]), "no MCS/GMCS"
                assert gatt.has_service(le.SVC["GTBS"]) or gatt.has_service(le.SVC["TBS"]), "no GTBS/TBS"

            check("topology", _topology)

            async def _coex():
                coex_svc = "a10c0001-0000-1000-8000-00805f9b34fb"
                coex_chr = "a10c0002-0000-1000-8000-00805f9b34fb"
                assert gatt.has_service(coex_svc), "custom coexistence service missing"
                val = await gatt.read(coex_chr)
                assert val.rstrip(b"\x00") == b"coexist", f"coex char value {val!r} != b'coexist'"

            await acheck("coexistence", _coex())

            async def _vcp():
                assert gatt.has_char(le.CHR["VOL_STATE"]), "no Volume State"
                await probe("VCS Volume State read", gatt.read(le.CHR["VOL_STATE"]))
                await probe("VCS Volume Flags read", gatt.read(le.CHR["VOL_FLAGS"]))
                await probe("VOCS Offset State read", gatt.read(le.CHR["VOCS_STATE"]))
                for i, c in enumerate(gatt.chars(le.CHR["AICS_STATE"])):
                    await probe(f"AICS[{i}] Input State read h={c.handle}", gatt.read_h(c.handle))
                await le.vcp_set_absolute_volume(gatt, 200)
                dut.expect_exact("[DUT] VCP state vol=200", timeout=8)
                state = await gatt.read(le.CHR["VOL_STATE"])
                assert state and state[0] == 200, f"Volume State {state!r} not 200"

            await acheck("vcp", _vcp())

            async def _micp():
                assert gatt.has_char(le.CHR["MIC_MUTE"]), "no MICS Mute"
                await probe("MICS Mute read", gatt.read(le.CHR["MIC_MUTE"]))
                await gatt.write(le.CHR["MIC_MUTE"], bytes([1]))
                dut.expect_exact("[DUT] MICP mute=1", timeout=8)
                mute = await gatt.read(le.CHR["MIC_MUTE"])
                assert mute and mute[0] == 1, f"MICS mute {mute!r} != 1"

            await acheck("micp", _micp())

            async def _mcp():
                assert gatt.has_char(le.CHR["MEDIA_CP"]), "no Media Control Point"
                await probe("GMCS Player Name read", gatt.read(le.CHR["MEDIA_PLAYER_NAME"]))
                await probe("GMCS Media State read", gatt.read(le.CHR["MEDIA_STATE"]))
                await gatt.subscribe(le.CHR["MEDIA_CP"])
                try:
                    await gatt.write(le.CHR["MEDIA_CP"], bytes([le.MCP_OP_PLAY]))
                    st = await gatt.read(le.CHR["MEDIA_STATE"])
                    assert st, "Media State not readable"
                finally:
                    await gatt.unsubscribe(le.CHR["MEDIA_CP"])

            await acheck("mcp", _mcp())

            async def _ccp():
                assert gatt.has_char(le.CHR["CALL_CP"]), "no Call Control Point"
                calls = le.parse_call_state(await gatt.read(le.CHR["CALL_STATE"]))
                LOGGER.info("CCP call state: %s", calls)
                assert calls, "no announced call in Call State"
                call_index = calls[0][0]

                states = await gatt.subscribe(le.CHR["CALL_STATE"])
                results = await gatt.subscribe(le.CHR["CALL_CP"])

                async def cp(label, coro, expect_op):
                    del results[:]
                    await coro
                    await asyncio.sleep(1.0)
                    assert results, f"{label}: no Call Control Point notification"
                    res = results[0]
                    assert len(res) >= 3, f"{label}: short notification {res.hex()}"
                    assert res[0] == expect_op, f"{label}: echoed opcode 0x{res[0]:02x}"
                    assert res[2] == 0x00, f"{label}: result code 0x{res[2]:02x} (notif {res.hex()})"

                try:
                    del results[:]
                    await le.ccp_originate(gatt, "tel:5551212")
                    await asyncio.sleep(1.0)
                    assert results, "originate: no Call Control Point notification"
                    assert results[0][2] == 0x06, (
                        f"originate: expected Invalid Outgoing URI, got notif {results[0].hex()}"
                    )

                    await cp("accept", le.ccp_accept(gatt, call_index), le.CCP_OP_ACCEPT)
                    await cp("terminate", le.ccp_terminate(gatt, call_index), le.CCP_OP_TERMINATE)
                    dut.expect(rf"\[DUT\] CCP terminated idx={call_index}", timeout=8)
                    LOGGER.info("CCP call state notifications: %s", [s.hex() for s in states])
                finally:
                    await gatt.unsubscribe(le.CHR["CALL_CP"])
                    await gatt.unsubscribe(le.CHR["CALL_STATE"])

            await acheck("ccp", _ccp())

            async def _csip():
                size = await gatt.read(le.CHR["SET_SIZE"])
                rank = await gatt.read(le.CHR["RANK"])
                sirk = await gatt.read(le.CHR["SIRK"])
                assert size and size[0] == 2, f"set size {size!r} != 2"
                assert rank and rank[0] == 1, f"rank {rank!r} != 1"
                assert sirk and len(sirk) >= 16, f"SIRK too short: {sirk!r}"

            await acheck("csip", _csip())

            def _cap():
                if not local_svcs:
                    # No DUT-side table: only the host view of the included CSIS is checkable.
                    assert gatt.has_service(le.SVC["CSIS"]), "host cannot see the CSIS that CAS includes"
                    return
                assert "CAS" in local_svcs, "DUT did not report CAS"
                cas_rc, cas_handle = local_svcs["CAS"]
                assert cas_rc == 0, f"CAS not in the DUT's GATT table (rc={cas_rc})"
                csis_rc, csis_handle = local_svcs["CSIS"]
                assert csis_rc == 0, f"CSIS not in the DUT's GATT table (rc={csis_rc})"
                assert cas_handle > csis_handle, (
                    f"CAS at {cas_handle} precedes CSIS at {csis_handle}; the include cannot resolve"
                )
                assert gatt.has_service(le.SVC["CSIS"]), "CAS includes CSIS but the host cannot see CSIS"

            check("cap", _cap)

            async def _has():
                active0 = await gatt.read(le.CHR["HA_ACTIVE_PRESET"])
                assert active0 and active0[0] == 1, f"initial active preset {active0!r} != 1"
                await gatt.subscribe(le.CHR["HA_PRESET_CP"])
                await gatt.subscribe(le.CHR["HA_ACTIVE_PRESET"])
                try:
                    await le.has_set_active_preset(gatt, 2)
                    dut.expect_exact("[DUT] HAS preset select idx=2", timeout=8)
                    active1 = await gatt.read(le.CHR["HA_ACTIVE_PRESET"])
                    assert active1 and active1[0] == 2, f"active preset {active1!r} != 2"
                finally:
                    await gatt.unsubscribe(le.CHR["HA_ACTIVE_PRESET"])
                    await gatt.unsubscribe(le.CHR["HA_PRESET_CP"])

            await acheck("has", _has())

            async def _tmap():
                role = await gatt.read(le.CHR["TMAP_ROLE"])
                assert role and len(role) >= 2, f"TMAP role too short: {role!r}"
                bits = role[0] | (role[1] << 8)
                assert (bits & 0x02) and (bits & 0x08), f"TMAP roles 0x{bits:04x} missing CT|UMR"

            await acheck("tmap", _tmap())

            async def _gmap():
                if not local_svcs and not gatt.has_service(le.SVC["GMAS"]):
                    LOGGER.info("  gmap: GMAS not exposed and no DUT-side table to tell why, skipping")
                    return
                if local_svcs and local_svcs["GMAS"][0] != 0:
                    LOGGER.info("  gmap: GMAS not in the DUT's GATT table (GMAP not compiled in), skipping")
                    return
                assert gatt.has_service(le.SVC["GMAS"]), "DUT registered GMAS but it is not exposed"
                role = await gatt.read(le.CHR["GMAP_ROLE"])
                assert role and role[0] & 0x02, f"GMAP role {role!r} missing UGT"

            await acheck("gmap", _gmap())

        finally:
            await gatt.disconnect()

        if errors:
            raise AssertionError("; ".join(f"{lbl}: {msg}" for lbl, msg in errors))


def _phase_control_gatt(dut, name, record):
    m = dut.expect(r"\[DUT\] (ControlGatt ready|ControlGatt not supported|ControlGatt .+ FAILED)", timeout=30)
    text = m.group(0)
    if b"not supported" in text:
        raise PhaseSkip("LE Audio engine not compiled in")
    assert b"ready" in text, f"DUT control_gatt did not come up: {text!r}"

    # CAS carries no characteristics; take the DUT's own GATT table as source of
    # truth for CAS (may not appear in host enumeration). Only NimBLE builds can
    # report it; the others print "localsvc unsupported" and local_svcs stays empty.
    local_svcs = {}
    for _ in range(11):
        lm = dut.expect(
            r"\[DUT\] localsvc (unsupported|(\w+) uuid=0x([0-9a-f]{4}) rc=(-?\d+) handle=(\d+))", timeout=10
        )
        if lm.group(1) == b"unsupported":
            break
        local_svcs[lm.group(2).decode()] = (int(lm.group(4)), int(lm.group(5)))
    LOGGER.info("DUT-side service table: %s", local_svcs or "unavailable (non-NimBLE build)")
    record("control_gatt.local_svcs", str(local_svcs) if local_svcs else "unavailable")

    _require_bumble()
    _run(_control_gatt(dut, name, record, local_svcs))


# ---------------------------------------------------------------------------
# Phase 2 — Auracast announcement discovery
# ---------------------------------------------------------------------------


async def _broadcast_announcement(broadcast_id: int):
    async with le.bumble_session() as device:
        return await le.find_broadcast(device, broadcast_id, timeout=25.0)


def _phase_broadcast_announcement(dut, name, record):
    m = dut.expect(
        r"\[DUT\] (BcastAnnounce ready id=0x[0-9a-fA-F]+ pbp=(\d)|BcastAnnounce not supported|BcastAnnounce .+ FAILED)",
        timeout=30,
    )
    if b"not supported" in m.group(0):
        raise PhaseSkip("LE Audio engine not compiled in")
    assert b"ready" in m.group(0), f"broadcast source did not start: {m.group(0)!r}"
    pbp = int(m.group(2)) == 1
    _require_bumble()
    ann = _run(_broadcast_announcement(BROADCAST_ID))
    assert ann, f"Broadcast Audio Announcement id=0x{BROADCAST_ID:06x} not seen"
    record("broadcast_announcement.broadcast_id", f"0x{ann['bid']:06x}")
    record("broadcast_announcement.public_broadcast", str(ann["has_pba"]))
    assert ann["bid"] == BROADCAST_ID, f"broadcast id 0x{ann['bid']:06x} != 0x{BROADCAST_ID:06x}"
    if pbp:
        assert ann["has_pba"], "DUT has PBP but advertises no Public Broadcast Announcement (0x1856)"


# ---------------------------------------------------------------------------
# Phase 3 — unicast LC3 data plane (Bumble ASCS/CIS client)
# ---------------------------------------------------------------------------


async def _unicast_audio(name: str) -> dict:
    async with le.bumble_session() as device:
        roles = le.controller_iso_roles(device)
        LOGGER.info("controller ISO roles: %s", roles)
        if "cis-central" not in roles:
            raise PhaseSkip(f"controller lacks CIS Central (roles={roles})")
        try:
            le.require_iso_hci(device, "unicast_audio")
        except RuntimeError as e:
            raise PhaseSkip(str(e)) from e
        address = await le.scan_by_name(device, name, timeout=20.0)
        assert address, f"DUT '{name}' not found for audio connect"
        LOGGER.info("unicast: connecting to %s", address)
        # Allow connect retries (HCI 0x3E) + ASCS/CIS/LC3 within the budget.
        return await asyncio.wait_for(
            le.unicast_client_stream(
                device, address, name=name, tone_seconds=5.0, collect_source=True
            ),
            timeout=120.0,
        )


def _phase_unicast_audio(dut, name, record):
    m = dut.expect(
        r"\[DUT\] (UnicastAudio ready player=\d+ recorder=\d+|UnicastAudio not supported|UnicastAudio .+ FAILED)",
        timeout=30,
    )
    if b"not supported" in m.group(0):
        raise PhaseSkip("LC3 codec (esp_audio_codec) not compiled in")
    assert b"ready" in m.group(0), f"unicast audio did not come up: {m.group(0)!r}"
    _require_bumble()

    try:
        result = _run(_unicast_audio(name))
    except PhaseSkip:
        raise
    except BaseException as e:  # noqa: BLE001 — CancelledError is BaseException
        raise AssertionError(f"unicast client stream failed: {e}") from e

    record("unicast_audio.sink_frames_sent", str(result["sink_frames_sent"]))
    record("unicast_audio.record_rms", f"{result['source_rms']:.1f}")
    record("unicast_audio.record_tone_ratio", f"{result['source_tone_ratio']:.3f}")

    # Ask DUT for decode metrics after we have been streaming.
    dut.write("REPORT\n")
    r = dut.expect(r"\[DUT\] UnicastAudio result samples=(\d+) meanamp=(\d+)", timeout=15)
    samples = int(r.group(1))
    meanamp = int(r.group(2))
    record("unicast_audio.decoded_samples", str(samples))
    record("unicast_audio.decoded_meanamp", str(meanamp))

    assert result["sink_frames_sent"] > 0, "host sent no LC3 frames toward the sink ASE"
    assert samples > 0, "DUT decoded no PCM samples from the host stream"
    assert meanamp > 500, f"decoded audio too quiet (meanamp={meanamp}); tone did not survive LC3"
    # The DUT's Recorder streams a 1 kHz tone on the source ASE.
    assert result["source_pcm"], "no audio received from the DUT's source ASE"
    assert result["source_rms"] > 200, f"recorded DUT source too quiet (rms={result['source_rms']:.1f})"


# ---------------------------------------------------------------------------
# Phase 4 — broadcast LC3 data plane (Bumble BIG sync)
# ---------------------------------------------------------------------------


async def _broadcast_audio() -> dict:
    async with le.bumble_session() as device:
        roles = le.controller_iso_roles(device)
        LOGGER.info("controller ISO roles: %s", roles)
        if "sync-receiver" not in roles:
            raise PhaseSkip(f"controller lacks Synchronized Receiver (roles={roles})")
        try:
            le.require_iso_hci(device, "broadcast_audio")
        except RuntimeError as e:
            raise PhaseSkip(str(e)) from e
        return await le.broadcast_sink_capture(
            device, BROADCAST_ID, capture_seconds=5.0, sync_timeout=25.0
        )


def _phase_broadcast_audio(dut, name, record):
    m = dut.expect(
        r"\[DUT\] (BcastAudio ready recorder=\d+ id=0x[0-9a-fA-F]+.*|BcastAudio not supported|BcastAudio .+ FAILED)",
        timeout=30,
    )
    if b"not supported" in m.group(0):
        raise PhaseSkip("LC3 codec (esp_audio_codec) not compiled in")
    assert b"ready" in m.group(0), f"broadcast audio did not come up: {m.group(0)!r}"
    _require_bumble()

    dut.write("REPORT\n")
    rr = dut.expect(r"\[DUT\] BcastAudio result recorder=(\d+)", timeout=15)
    assert int(rr.group(1)) == 1, "DUT Recorder failed to start on the BIS"

    bumble_err = None
    try:
        result = _run(_broadcast_audio())
        record("broadcast_audio.record_rms", f"{result['rms']:.1f}")
        record("broadcast_audio.record_tone_ratio", f"{result['tone_ratio']:.3f}")
        assert result["rms"] > 200, f"received broadcast too quiet (rms={result['rms']:.1f}); BIS tone did not arrive"
        record("broadcast_audio.path", "bumble")
        return
    except PhaseSkip as e:
        bumble_err = e
        LOGGER.warning("Bumble broadcast sink unavailable (%s); trying phone assist", e)

    if not _phone_assist():
        raise bumble_err or PhaseSkip("Bumble cannot BIG-sync; set BLE_AUDIO_PHONE=1 to use a Galaxy S23/S24")

    prompt = (
        f"PHONE as Auracast SINK (ESP is source '{name}', id=0x{BROADCAST_ID:06x}):\n"
        "  Galaxy S23/S24, One UI 6.1+: Quick Settings → Audio broadcast → Listen\n"
        "  (or Settings → Connections → Bluetooth → ⋮ → Listen to Auracast broadcast).\n"
        f"  Join '{name}'. No password. You should hear a 1 kHz tone.\n"
        "  Compatible Galaxy Buds work; some One UI builds also play on the phone."
    )
    if not _wait_phone_ok(prompt):
        raise PhaseSkip(
            f"phone did not confirm hearing '{name}' within timeout "
            f"(Bumble: {bumble_err})"
        )
    record("broadcast_audio.path", "phone_sink")
    record("broadcast_audio.phone_heard", "PASS")


# ---------------------------------------------------------------------------
# Phase 5 — Broadcast Audio Scan Service (Scan Delegator) over GATT
# ---------------------------------------------------------------------------


async def _scan_delegator(name, record):
    async with le.bumble_session() as device:
        address = await le.scan_by_name(device, name, timeout=20.0)
        assert address, f"DUT '{name}' not found in scan"
        gatt = await le.Gatt.connect(device, address, timeout=25.0, name=name)
        try:
            paired = await gatt.pair()
            LOGGER.info(
                "scan_delegator pair() -> %s encrypted=%s",
                paired,
                getattr(gatt.connection, "is_encrypted", None),
            )
            LOGGER.info("scan_delegator discovered services: %s", sorted(gatt.service_uuids()))
            assert gatt.has_service(le.SVC["BASS"]), "no Broadcast Audio Scan Service"
            assert gatt.has_char(le.CHR["BASS_RX_STATE"]), "no Broadcast Receive State"
            assert gatt.has_char(le.CHR["BASS_CP"]), "no Broadcast Audio Scan Control Point"
            record("scan_delegator.topology", "PASS")

            _ = await gatt.read(le.CHR["BASS_RX_STATE"])
            # Do not write Remote Scan Stopped (0x00): that tells the
            # delegator the assistant finished scanning and can cancel the
            # sink's own extended scan before the phone test. Discovery of
            # BASS_CP is enough to prove the characteristic is present.
            record("scan_delegator.bass_cp", "PASS")
        finally:
            await gatt.disconnect()


def _phase_scan_delegator(dut, name, record):
    m = dut.expect(
        r"\[DUT\] (ScanDelegator ready.*|ScanDelegator not supported|ScanDelegator .+ FAILED)",
        timeout=30,
    )
    if b"not supported" in m.group(0):
        raise PhaseSkip("LE Audio engine not compiled in")
    assert b"ready" in m.group(0), f"scan delegator did not come up: {m.group(0)!r}"
    _require_bumble()
    _run(_scan_delegator(name, record))

    if not _phone_assist():
        record("scan_delegator.phone_source", "SKIP: BLE_AUDIO_PHONE=0")
        return

    prompt = (
        f"PHONE as Auracast SOURCE (ESP is broadcast sink '{name}'):\n"
        "  1. Stop Listen on the Buds.\n"
        f"  2. Bluetooth settings → pair/connect to '{name}' (LE Audio / Audio sink).\n"
        "  3. Then: Settings → Connections → Bluetooth → ⋮ →\n"
        "     Broadcast sound using Auracast. Leave the password EMPTY (no lock).\n"
        "  4. Start broadcast and play music/video.\n"
        "  Samsung uses BASS on the connected ESP; TX-only without that connect does not sync."
    )
    LOGGER.warning("%s", prompt)
    print("\n" + prompt + "\n", flush=True)

    deadline = time.time() + 180.0
    received = 0
    streaming = 0
    while time.time() < deadline:
        dut.write("REPORT\n")
        try:
            r = dut.expect(r"\[DUT\] BcastSink result received=(\d+) streaming=(\d+)", timeout=5)
            received = int(r.group(1))
            streaming = int(r.group(2))
            LOGGER.info("bcast sink received=%d streaming=%d", received, streaming)
            if received >= 20:
                break
        except Exception:  # noqa: BLE001
            pass
        time.sleep(2.0)

    record("scan_delegator.phone_received", str(received))
    record("scan_delegator.phone_streaming", str(streaming))
    if received < 20:
        raise PhaseSkip(
            f"ESP broadcast sink got received={received} (need >= 20 BIS SDUs). "
            "Confirm the phone is broadcasting with no password and playing audio."
        )
    record("scan_delegator.phone_source", "PASS")


# ---------------------------------------------------------------------------
# Phase 6 — memory release + reinit guard
# ---------------------------------------------------------------------------


def _phase_memory_release(dut, name, record):
    dut.expect(r"\[DUT\] Heap before release: \d+", timeout=30)
    dut.expect(r"\[DUT\] Heap after release: \d+", timeout=10)
    m = dut.expect(r"\[DUT\] Memory freed: (-?\d+) bytes", timeout=10)
    freed = int(m.group(1))
    assert freed >= 10240, f"expected >= 10240 bytes freed, got {freed}"
    dut.expect_exact("[DUT] Memory release OK", timeout=10)
    dut.expect_exact("[DUT] Reinit blocked OK", timeout=10)
    dut.expect_exact("[DUT] All phases complete", timeout=10)


# ---------------------------------------------------------------------------
# Single test function
# ---------------------------------------------------------------------------


def test_ble_audio_host(dut, ci_job_id, record_property):
    if not le.adapter_present():
        pytest.skip("no Bluetooth controller present on the host (bench-only suite)")
    if not le.bumble_available():
        pytest.skip("bumble[auracast] / lc3 not installed (see requirements.txt)")

    name = "BLEAH_" + (ci_job_id if ci_job_id else rand_str4())
    LOGGER.info("LE Audio host-interop test name: %s", name)
    LOGGER.info("BUMBLE_TRANSPORT=%s", le.default_transport())
    if PHASE_SOFT_REBOOT:
        LOGGER.info("PHASE_SOFT_REBOOT=1 (one audio phase per boot until same-boot re-init passes on the bench)")
    if _phone_assist():
        LOGGER.info(
            "BLE_AUDIO_PHONE=1: keep a Galaxy S23/S24 ready. Phase 4 = phone Listen; "
            "phase 5 = phone Broadcast. Confirm Listen with Enter or touch %s",
            PHONE_OK_MARKER,
        )

    _handshake_name(dut, name, timeout=120)

    phases = {
        1: _phase_control_gatt,
        2: _phase_broadcast_announcement,
        3: _phase_unicast_audio,
        4: _phase_broadcast_audio,
        5: _phase_scan_delegator,
        6: _phase_memory_release,
    }

    passed, skipped, failed = [], [], []

    for num, fn in phases.items():
        label = PHASE_LABELS[num]
        LOGGER.info("Running phase %d: %s", num, label)
        if not _start_phase(dut, num, name=name):
            prev = PHASE_LABELS[num - 1]
            failed.append((f"{prev} teardown", "no done marker (teardown crashed or hung)"))
            record_property(f"phase_{prev}_teardown", "FAIL: no done marker")
        try:
            fn(dut, name, record_property)
            passed.append(label)
            record_property(f"phase_{label}", "PASS")
            LOGGER.info("PASSED: phase %d (%s)", num, label)
        except PhaseSkip as e:
            skipped.append(label)
            record_property(f"phase_{label}", f"SKIP: {e}")
            LOGGER.info("SKIPPED: phase %d (%s): %s", num, label, e)
        except Exception as e:  # noqa: BLE001
            failed.append((label, str(e)))
            record_property(f"phase_{label}", f"FAIL: {e}")
            LOGGER.error("FAILED: phase %d (%s): %s", num, label, e)

    summary = f"{len(passed)} passed, {len(skipped)} skipped, {len(failed)} failed out of {len(PHASE_LABELS)}"
    LOGGER.info("Summary: %s", summary)
    record_property("summary", summary)

    if failed:
        lines = [f"  {lbl}: {err}" for lbl, err in failed]
        pytest.fail(f"{len(failed)} phase(s) failed:\n" + "\n".join(lines))
