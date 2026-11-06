# BT Memory Wrap Validation Test

Validates the `--wrap` linker intercepts on `esp_bt_mem_release` and `esp_bt_controller_mem_release` that provide double-free protection and cross-API tracking for Bluetooth memory management. Each phase runs after a hard reset to ensure fresh BT memory state.

## Test Cases

| Phase | Description |
|---|---|
| 1 | `btMemRelease()` Arduino API succeeds, updates tracking flag, second call is a safe no-op |
| 2 | Direct `esp_bt_controller_mem_release()` updates tracking via `__wrap`, double-release guarded |
| 3 | Direct `esp_bt_mem_release()` (host+controller combined) updates tracking, double-release guarded |
| 4 | Cross-API double-free: `btMemRelease()` then `esp_bt_controller_mem_release()` — second is no-op |
| 5 | Cross-API double-free (reversed): `esp_bt_mem_release()` then `btMemRelease()` — second is no-op |
| 6 | `btMarkMemReleased()` sentinel marks memory released without real call; all subsequent releases are no-ops |
| 7 | `btStartMode(BLE)` fails after `btMemRelease(BLE)` — freed memory cannot be reclaimed |
| 8 | Full lifecycle: `btStart()` → `btMemRelease()` rejected while running → `btStop()` → `btMemRelease()` succeeds |
| 9 | Matter-style release: `esp_bt_controller_mem_release(CLASSIC_BT)` frees Classic BT only, BLE remains usable (skipped on non-BTDM chips) |
| 10 | `btInUse()` pointer comparison: default weak alias detected, memory governed by sub-functions (`bleInUse`/`btClassicInUse`) |

## Requirements

- **Hardware**: One board with BLE support (e.g. ESP32, ESP32-S3)
- **Wokwi/QEMU**: Not supported
- **SoC Config**: `CONFIG_SOC_BLE_SUPPORTED=y`, `CONFIG_BT_CONTROLLER_ENABLED=y`

## Serial Protocol

1. DUT prints `[BT_MEM_WRAP] Ready` and `[BT_MEM_WRAP] Send phase:`
2. pytest sends `PHASE N` for each phase
3. DUT runs the phase, prints individual `PASS`/`FAIL` checks
4. DUT prints `[BT_MEM_WRAP] Phase N: PASSED` or `FAILED`
5. DUT calls `ESP.restart()` after each phase to reset BT memory state

## Notes

- The sketch includes both `esp32-hal-alloc-bt-classic-mem.h` and `esp32-hal-alloc-ble-mem.h` to prevent `initArduino()` from auto-releasing BT memory before the test runs.
- Phase 9 is only active on BTDM-capable chips (ESP32) where Classic BT is supported; it is a compile-time skip on BLE-only chips.
