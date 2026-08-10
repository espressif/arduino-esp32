# OTA (Unsigned) Validation Test

Validates the `Update` API and `HTTPUpdate` library for unsigned OTA workflows: begin/abort, error handling, MD5 verification, and HTTP-based firmware download.

## Test Cases

| Test Function | Description |
|---|---|
| `test_update_begin_abort` | `Update.begin` + `abort`, verify update is not running |
| `test_update_error_no_begin` | `Update.write` without `begin` returns 0 |
| `test_update_md5_check` | `setMD5` on an active update session |
| `test_httpupdate_invalid_url` | `HTTPUpdate` to unreachable URL (192.0.2.1) returns error |
| `test_httpupdate_download` | `HTTPUpdate` download from pytest HTTP server |

## Requirements

- **Hardware**: Any ESP32 variant with Wi-Fi support
- **Wokwi/QEMU**: Not supported
- **CI Runner**: `wifi_router`
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`

## Serial Protocol

1. DUT prints `OTA_READY`
2. DUT prints `Send SSID:` → host sends SSID
3. DUT prints `Send Password:` → host sends password
4. DUT prints `Send Server URL (or NONE):` → host sends URL or `NONE`
5. Unity test suite runs

## Notes

- The pytest script starts a local HTTP server serving the build directory (which contains the firmware `.bin`). The test fails if the server cannot be started, the LAN IP cannot be determined, or the device cannot connect to the server.
- `httpUpdate.rebootOnUpdate(false)` is set so the DUT does not reboot mid-test.
- Signed OTA verification is covered by the separate `signed_ota/` test suite.
