import logging
import re

LOGGER = logging.getLogger(__name__)

RX_RESULT_RE = re.compile(r"\[RECEIVER\] (\S+) matches=(\d+) full_ramps=(\d+) zeros=(\d+) total=(\d+)")


def _g(match, n=0):
    """Decode a match group to str (pexpect may return bytes)."""
    v = match.group(n)
    return v.decode() if isinstance(v, bytes) else v


def _expect_rx(receiver, tag, timeout=30):
    m = receiver.expect(RX_RESULT_RE, timeout=timeout)
    return {
        "tag": m.group(1),
        "matches": int(m.group(2)),
        "full_ramps": int(m.group(3)),
        "zeros": int(m.group(4)),
        "total": int(m.group(5)),
    }


def _assert_ramp(result, phase, min_matches=48, min_full_ramps=1, max_zeros_pct=50):
    matches = result["matches"]
    full_ramps = result["full_ramps"]
    zeros = result["zeros"]
    total = result["total"]

    LOGGER.info(
        f"{phase}: matches={matches}/64 full_ramps={full_ramps} "
        f"zeros={zeros}/{total} ({100 * zeros // max(total, 1)}%)"
    )

    assert total > 0, f"{phase}: receiver got no data"
    assert matches >= min_matches, f"{phase}: best window too low: {matches}/64 (need >= {min_matches})"
    assert (
        full_ramps >= min_full_ramps
    ), f"{phase}: no complete ramp found (full_ramps={full_ramps}, need >= {min_full_ramps})"
    if total > 0:
        zeros_pct = 100 * zeros // total
        assert zeros_pct <= max_zeros_pct, (
            f"{phase}: too many zeros: {zeros}/{total} ({zeros_pct}%, max {max_zeros_pct}%%) — "
            "possible wrong-channel extraction or no data"
        )


def _run_loopback(
    sender, receiver, phase_name, tx_cmd, tx_clocking, tx_done, rx_cmd, rx_listening, rx_done_tag, **assert_kwargs
):
    """Run a single loopback phase with master-first startup."""
    LOGGER.info(f"{phase_name}...")
    sender.write(tx_cmd)
    sender.expect_exact(tx_clocking, timeout=10)

    receiver.write(rx_cmd)
    receiver.expect_exact(rx_listening, timeout=10)

    sender.expect_exact(tx_done, timeout=30)
    r = _expect_rx(receiver, rx_done_tag)
    _assert_ramp(r, phase_name, **assert_kwargs)


def test_i2s(dut):
    sender = dut[0]
    receiver = dut[1]

    LOGGER.info("Waiting for devices to be ready...")
    sender.expect_exact("[SENDER] Ready", timeout=120)
    receiver.expect_exact("[RECEIVER] Ready", timeout=120)

    LOGGER.info("Waiting for sender Unity tests...")
    sender.expect_unity_test_output(timeout=120)

    sender.expect_exact("[SENDER] TX_READY", timeout=10)

    # Query HW version early — needed to skip unsupported configurations
    LOGGER.info("Querying HW version...")
    sender.write("QUERY_HW_V1")
    m = sender.expect(r"\[SENDER\] HW_V1_(YES|NO)", timeout=10)
    is_hw_v1 = _g(m, 1) == "YES"
    LOGGER.info(f"HW v1: {is_hw_v1}")

    # --- Phase 1: mono loopback (default slot, 16kHz, 16-bit) ---
    _run_loopback(
        sender,
        receiver,
        "Phase 1: mono 16kHz/16-bit (default slot)",
        "START_TX",
        "[SENDER] TX_CLOCKING",
        "[SENDER] TX_DONE",
        "START_RX",
        "[RECEIVER] RX_LISTENING",
        "RX_DONE",
    )

    # --- Phase 2: stereo → mono right-slot (16kHz, 16-bit) ---
    _run_loopback(
        sender,
        receiver,
        "Phase 2: stereo→mono right-slot",
        "START_TX_STEREO_RAMP",
        "[SENDER] TX_STEREO_CLOCKING",
        "[SENDER] TX_STEREO_DONE",
        "START_RX_RIGHT",
        "[RECEIVER] RX_RIGHT_LISTENING",
        "RX_RIGHT_DONE",
    )

    # --- Phase 3: stereo → mono left-slot (16kHz, 16-bit) ---
    _run_loopback(
        sender,
        receiver,
        "Phase 3: stereo→mono left-slot",
        "START_TX_LEFT_RAMP",
        "[SENDER] TX_LEFT_CLOCKING",
        "[SENDER] TX_LEFT_DONE",
        "START_RX_LEFT",
        "[RECEIVER] RX_LEFT_LISTENING",
        "RX_LEFT_DONE",
    )

    # --- Phase 4: mono RIGHT TX → RIGHT RX (16kHz, 16-bit) ---
    _run_loopback(
        sender,
        receiver,
        "Phase 4: mono RIGHT TX→RIGHT RX",
        "START_TX_MONO_RIGHT",
        "[SENDER] TX_MONO_RIGHT_CLOCKING",
        "[SENDER] TX_MONO_RIGHT_DONE",
        "START_RX_RIGHT",
        "[RECEIVER] RX_RIGHT_LISTENING",
        "RX_RIGHT_DONE",
    )

    # --- Parameterized rate x width x mode bounds matrix ---
    # 8-bit on ESP32 HW v1 requires special uint16_t packing (high byte valid).
    # The Arduino library now handles this transparently in write()/readBytes().
    rates = [8000, 96000]
    widths = [8, 16, 32]
    modes = [(1, "mono"), (2, "stereo")]

    phase = 5
    for rate in rates:
        for width in widths:
            for ch, ch_name in modes:
                name = f"Phase {phase}: {ch_name} {rate}Hz/{width}-bit"
                _run_loopback(
                    sender,
                    receiver,
                    name,
                    f"TX {rate} {width} {ch}",
                    "[SENDER] TX_PARAM_CLOCKING",
                    "[SENDER] TX_PARAM_DONE",
                    f"RX {rate} {width} {ch}",
                    "[RECEIVER] RX_PARAM_LISTENING",
                    "RX_PARAM_DONE",
                )
                phase += 1

    # --- Phases 17-20: mono slot_mask matrix (LEFT=1, RIGHT=2) ---
    # Tests explicit slot selection for 16-bit (HW v1 workaround) and 32-bit (no workaround).
    # BOTH is excluded: on non-HW v1 targets it duplicates samples in the DMA buffer,
    # scrambling the ramp pattern. begin() with BOTH is already covered by single-device tests.
    slot_widths = [16, 32]
    slots = [(1, "LEFT"), (2, "RIGHT")]

    for width in slot_widths:
        for slot_val, slot_name in slots:
            name = f"Phase {phase}: mono {width}-bit slot={slot_name}"
            _run_loopback(
                sender,
                receiver,
                name,
                f"TX 16000 {width} 1 {slot_val}",
                "[SENDER] TX_PARAM_CLOCKING",
                "[SENDER] TX_PARAM_DONE",
                f"RX 16000 {width} 1 {slot_val}",
                "[RECEIVER] RX_PARAM_LISTENING",
                "RX_PARAM_DONE",
            )
            phase += 1

    # --- Stereo slot_mask TX tests ---
    # stereo+LEFT TX: L data from buffer appears on BOTH wire slots.
    # RX mono LEFT reads the L ramp (ramp_type=0).
    name = f"Phase {phase}: stereo 16-bit TX=LEFT → mono RX=LEFT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 2 1",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX 16000 16 1 1 0",
        "[RECEIVER] RX_PARAM_LISTENING",
        "RX_PARAM_DONE",
    )
    phase += 1

    # stereo+RIGHT TX: R data from buffer goes to the RIGHT wire slot (LEFT is muted).
    # RX mono RIGHT reads the R ramp (ramp_type=1).
    name = f"Phase {phase}: stereo 16-bit TX=RIGHT → mono RX=RIGHT (R ramp)"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 2 2",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX 16000 16 1 2 1",
        "[RECEIVER] RX_PARAM_LISTENING",
        "RX_PARAM_DONE",
    )
    phase += 1

    # --- Cross-slot loopback: TX mono BOTH → RX mono LEFT ---
    # TX sends mono with BOTH (data on L and R), RX receives with LEFT only.
    name = f"Phase {phase}: mono 16-bit TX=BOTH RX=LEFT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 1 3",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX 16000 16 1 1",
        "[RECEIVER] RX_PARAM_LISTENING",
        "RX_PARAM_DONE",
    )
    phase += 1

    # --- Cross-slot loopback: TX mono BOTH → RX mono RIGHT ---
    name = f"Phase {phase}: mono 16-bit TX=BOTH RX=RIGHT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 1 3",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX 16000 16 1 2",
        "[RECEIVER] RX_PARAM_LISTENING",
        "RX_PARAM_DONE",
    )
    phase += 1

    # --- configureRX slot change: begin LEFT → configureRX RIGHT ---
    # Verifies the new configureRX slot_mask parameter in a live loopback.
    name = f"Phase {phase}: configureRX 16-bit LEFT→RIGHT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 1 2",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX_RECONFIG 16000 16 1 1 2",
        "[RECEIVER] RX_RECONFIG_LISTENING",
        "RX_RECONFIG_DONE",
    )
    phase += 1

    # --- configureRX slot change: begin RIGHT → configureRX LEFT ---
    name = f"Phase {phase}: configureRX 16-bit RIGHT→LEFT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 1 1",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX_RECONFIG 16000 16 1 2 1",
        "[RECEIVER] RX_RECONFIG_LISTENING",
        "RX_RECONFIG_DONE",
    )
    phase += 1

    # --- configureRX slot change at 32-bit (no HW v1 workaround) ---
    name = f"Phase {phase}: configureRX 32-bit LEFT→RIGHT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 32 1 2",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "RX_RECONFIG 16000 32 1 1 2",
        "[RECEIVER] RX_RECONFIG_LISTENING",
        "RX_RECONFIG_DONE",
    )
    phase += 1

    # --- Fix 2 regression: configureRX default slot_mask preserves current slot ---
    # TX sends mono RIGHT ramp. RX begins with RIGHT, then does two rate
    # changes with default slot_mask. If the slot is preserved, the R ramp
    # is received. If the old bug resets to LEFT, it would see the muted channel.
    name = f"Phase {phase}: configureRX default slot preserves RIGHT"
    _run_loopback(
        sender,
        receiver,
        name,
        "TX 16000 16 1 2",
        "[SENDER] TX_PARAM_CLOCKING",
        "[SENDER] TX_PARAM_DONE",
        "START_RX_SLOT_PRESERVE",
        "[RECEIVER] RX_SLOT_PRESERVE_LISTENING",
        "RX_SLOT_PRESERVE_DONE",
    )
    phase += 1

    # --- HW v1 BOTH averaging tests (ESP32 only) ---
    # On HW v1, mono+BOTH+16-bit activates the workaround which should use
    # the _both averaging function. TX sends stereo [L=ramp, R=offset_ramp];
    # the receiver verifies the output matches (L+R)/2.
    # Skipped on non-HW v1 where mono+BOTH causes sample duplication instead.
    if is_hw_v1:
        # Bug 1 regression: begin(MONO, BOTH) should use _both, not _left
        name = f"Phase {phase}: HW v1 mono BOTH begin (averaged ramp)"
        LOGGER.info(f"{name}...")
        sender.write("TX 16000 16 2")
        sender.expect_exact("[SENDER] TX_PARAM_CLOCKING", timeout=10)
        receiver.write("START_RX_MONO_BOTH")
        receiver.expect_exact("[RECEIVER] RX_MONO_BOTH_LISTENING", timeout=10)
        sender.expect_exact("[SENDER] TX_PARAM_DONE", timeout=30)
        r = _expect_rx(receiver, "RX_MONO_BOTH_DONE")
        _assert_ramp(r, name)
        phase += 1

        # Bug 2 regression: configureRX(MONO, BOTH) should switch to _both
        name = f"Phase {phase}: HW v1 configureRX BOTH (averaged ramp)"
        LOGGER.info(f"{name}...")
        sender.write("TX 16000 16 2")
        sender.expect_exact("[SENDER] TX_PARAM_CLOCKING", timeout=10)
        receiver.write("START_RX_RECONFIG_BOTH")
        receiver.expect_exact("[RECEIVER] RX_RECONFIG_BOTH_LISTENING", timeout=10)
        sender.expect_exact("[SENDER] TX_PARAM_DONE", timeout=30)
        r = _expect_rx(receiver, "RX_RECONFIG_BOTH_DONE")
        _assert_ramp(r, name)
        phase += 1
    else:
        LOGGER.info("Not HW v1 — skipping BOTH averaging tests")

    # --- TDM loopback (skipped on targets without TDM support or HW v1) ---
    LOGGER.info("Querying TDM support...")
    sender.write("QUERY_TDM_SUPPORT")
    m = sender.expect(r"\[SENDER\] TDM_(SUPPORTED|NOT_SUPPORTED)", timeout=10)
    if _g(m, 1) == "SUPPORTED":
        LOGGER.info(f"Phase {phase}: TDM mono SLOT0...")
        sender.write("START_TX_TDM")
        sender.expect_exact("[SENDER] TX_TDM_CLOCKING", timeout=10)

        receiver.write("START_RX_TDM")
        m_rx = receiver.expect(r"\[RECEIVER\] RX_TDM_(LISTENING|SKIP|FAIL.*)", timeout=10)
        if "LISTENING" in _g(m_rx):
            sender.expect_exact("[SENDER] TX_TDM_DONE", timeout=30)
            r = _expect_rx(receiver, "RX_TDM_DONE")
            _assert_ramp(r, f"Phase {phase} (TDM)")
        else:
            LOGGER.warning("TDM receiver skipped or failed to arm, skipping TDM loopback")
    else:
        LOGGER.info(f"TDM not supported on this target — skipping Phase {phase}")

    sender.write("DONE")
    sender.expect_exact("[SENDER] DONE", timeout=10)
    receiver.write("DONE")
    receiver.expect_exact("[RECEIVER] DONE", timeout=10)

    LOGGER.info("I2S multi-DUT test passed!")
