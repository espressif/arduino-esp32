# BT InUse Override Validation Test

Validates the `btInUse()` strong-override backwards-compatibility path. The sketch provides a strong `btInUse()` returning `true` (simulating legacy user code) without including any BT memory allocation headers, verifying that `initArduino()` detects the override via pointer comparison and preserves all BT memory.

## Test Cases

| Test Function | Description |
|---|---|
| Pointer comparison | Verify `btInUse` does NOT point to `_btInUse_default` (strong override detected) |
| `bleInUse()` check | Confirm `bleInUse()` returns false (no alloc header included) |
| `btClassicInUse()` check | Confirm `btClassicInUse()` returns false (no alloc header included) |
| BLE memory preserved | BLE memory not released despite `bleInUse()=false` (protected by `btInUse()` override) |
| Classic BT memory preserved | Classic BT memory not released despite `btClassicInUse()=false` (on BTDM chips) |
| BT start/stop | `btStart()` succeeds (proving memory is intact), then `btStop()` cleans up |

## Requirements

- **Hardware**: One board with BLE support (e.g. ESP32, ESP32-S3)
- **Wokwi/QEMU**: Not supported
- **SoC Config**: `CONFIG_SOC_BLE_SUPPORTED=y`, `CONFIG_BT_CONTROLLER_ENABLED=y`

## Notes

- This test complements `bt_mem_wrap` by testing the legacy backwards-compatibility path where users provide a strong `btInUse()` function instead of using the modern per-mode allocation headers.
- The key invariant: if a user provides `btInUse() { return true; }`, **no** BT memory should be released by `initArduino()`, regardless of what `bleInUse()` and `btClassicInUse()` return.
