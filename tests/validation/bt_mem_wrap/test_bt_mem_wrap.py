import logging


def test_bt_mem_wrap(dut):
    LOGGER = logging.getLogger(__name__)

    def run_phase(n, description):
        LOGGER.info("Phase %d: %s", n, description)
        dut.expect_exact("[BT_MEM_WRAP] Ready", timeout=30)
        dut.expect_exact("[BT_MEM_WRAP] Send phase:")
        dut.write(f"PHASE {n}\n")
        dut.expect_exact(f"[BT_MEM_WRAP] Phase {n}: PASSED", timeout=30)
        LOGGER.info("Phase %d passed", n)

    # Phase 1: btMemRelease() Arduino API + double-free guard.
    run_phase(1, "btMemRelease API")

    # Phase 2: direct esp_bt_controller_mem_release() + double-free guard.
    # Simulates an external component (e.g. Matter) releasing memory directly.
    run_phase(2, "direct esp_bt_controller_mem_release")

    # Phase 3: direct esp_bt_mem_release() + double-free guard.
    run_phase(3, "direct esp_bt_mem_release")

    # Phase 4: cross-API double-free — btMemRelease() then direct
    # esp_bt_controller_mem_release().  Different wrap functions but the same
    # tracking flags, so the second call must be a no-op.
    run_phase(4, "cross-API double-free (btMemRelease then esp_bt_controller_mem_release)")

    # Phase 5: cross-API double-free (reversed) — direct esp_bt_mem_release()
    # then btMemRelease().
    run_phase(5, "cross-API double-free (esp_bt_mem_release then btMemRelease)")

    # Phase 6: btMarkMemReleased() as a sentinel — marks memory released without
    # a real ESP-IDF call; all subsequent wrap calls must be no-ops.
    run_phase(6, "btMarkMemReleased sentinel")

    # Phase 7: btStartMode(BLE) must fail after btMemRelease(BLE) — the freed
    # BLE memory cannot be reclaimed.  (btStart() may still succeed on BTDM
    # chips by downgrading to Classic-only; see phase 9.)
    run_phase(7, "btStartMode(BLE) fails after btMemRelease(BLE)")

    # Phase 8: full lifecycle — btStart() succeeds, btMemRelease() while running
    # is rejected without corrupting tracking state, btStop() cleans up, then
    # btMemRelease() finally succeeds.
    run_phase(8, "start, release-while-running rejected, stop, release")

    # Phase 9: Matter-style release — simulates BLEManagerImpl::InitESPBleLayer()
    # calling esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT) on ESP32.
    # The __wrap intercept must update only the Classic BT tracking flag and
    # leave BLE usable for commissioning.  On chips without Classic BT the
    # phase is a compile-time no-op and always passes.
    run_phase(9, "Matter-style Classic BT release (BLEManagerImpl simulation)")

    # Phase 10: btInUse() pointer comparison — no user override in this test.
    # Verifies that when btInUse resolves to the default weak alias (_btInUse_default),
    # the pointer comparison correctly identifies it as NOT a user override, and
    # memory release is governed solely by the sub-functions (bleInUse / btClassicInUse).
    # Since this test includes both alloc headers, both flags are true and
    # memory should NOT be released by initArduino().
    run_phase(10, "btInUse pointer comparison (no strong override)")

    LOGGER.info("BT memory wrap test passed!")
