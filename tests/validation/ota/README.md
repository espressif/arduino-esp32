# OTA (Unsigned) Validation Test

Validates the `Update` API, `HTTPUpdate` library, and `ArduinoOTA` for unsigned OTA workflows (IPv4 and IPv6).

## Test Cases

| Test Function | Description |
|---|---|
| `test_update_begin_abort` | `Update.begin` + `abort` (local flash; IP-independent) |
| `test_update_error_no_begin` | `Update.write` without `begin` returns 0 |
| `test_update_md5_check` | `setMD5` on an active update session |
| `test_update_sha256_format` | `setSHA256` accepts a 64-character hex digest and rejects non-hex input |
| `test_update_sha256_roundtrip` | Local (no network) `begin`/`write`/`end` round trip: correct digest succeeds, wrong digest fails with `UPDATE_ERROR_SHA256`, MD5+SHA-256 combined |
| `test_arduino_ota_begin_end` | `ArduinoOTA.begin` / `getHostname` / `end` |
| `test_httpupdate_invalid_url` | `HTTPUpdate` to unreachable IPv4 URL (192.0.2.1) |
| `test_httpupdate_invalid_url_ipv6` | `HTTPUpdate` to unreachable bracketed IPv6 URL (`[2001:db8::1]`) |
| `test_httpupdate_invalid_checksums_abort` | Invalid MD5/SHA-256 values abort the update session |
| `test_httpupdate_wrong_sha256_has_no_digest` | A SHA-256 mismatch fails without exposing a digest |
| `test_arduino_ota_upload_no_auth` | Full `espota.py` upload without password (IPv4) |
| `test_arduino_ota_ipv4_with_ipv6_enabled` | IPv4 upload after STA IPv6 is enabled (dual-stack bind) |
| `test_arduino_ota_upload_ipv6` | Full `espota.py` upload over global IPv6 (ignored if host/DUT lack it) |
| `test_arduino_ota_ipv4_after_ipv6` | IPv6 upload then IPv4 upload on a fresh `begin()` |
| `test_arduino_ota_ipv4_mapped` | Host uses `::ffff:a.b.c.d` literals against dual-stack DUT |
| `test_arduino_ota_ipv6_with_auth` | IPv6 upload with PBKDF2-HMAC-SHA256 auth |
| `test_arduino_ota_upload_with_auth` | IPv4 upload with PBKDF2-HMAC-SHA256 auth |
| `test_httpupdate_download` | `HTTPUpdate` download and SHA-256 verification over IPv4 |
| `test_httpupdate_download_ipv6` | `HTTPUpdate` download and SHA-256 verification via RFC 3986 IPv6 URL |

## Requirements

- **Hardware**: Any ESP32 variant with Wi-Fi support
- **Wokwi/QEMU**: Not supported
- **CI Runner**: `wifi_router`
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`
- **IPv6**: Native IPv6 cases need global IPv6 on DUT and host; mapped/dual-stack cases need host AF_INET6; otherwise those cases are ignored (not failed)

## Serial Protocol

1. DUT prints `OTA_READY`
2. DUT prints `Send SSID:` → host sends SSID
3. DUT prints `Send Password:` → host sends password
4. DUT prints `Send Server URL (or NONE):` → host sends IPv4 HTTP base URL or `NONE`
5. DUT prints `Send IPv6 Mode (NONE|MAPPED|FULL):`
   - `NONE` — host has no IPv6 sockets
   - `MAPPED` — AF_INET6 works (IPv4-mapped uploads OK), no global IPv6
   - `FULL` — host has a global IPv6 address for native IPv6 OTA/HTTP
6. DUT prints `Send IPv6 Server Host (or NONE):` → bare IPv6 host or `NONE`
7. DUT prints `Send IPv6 Server Port (or 0):` → TCP port or `0`
8. Unity test suite runs
9. During ArduinoOTA upload tests:
   - `ARDUINO_OTA_BEGIN <ip> <port> <NONE|password>` → host runs `tools/espota.py`
   - `ARDUINO_OTA_BEGIN_MAPPED <ipv4> <port> NONE` → host runs espota with `::ffff:` mapped addresses

## Notes

- The pytest HTTP server serves the firmware with its SHA-256 digest in an `x-SHA256` response header, prefers dual-stack (`::` with `IPV6_V6ONLY=0`), and falls back to IPv4-only if IPv6 bind fails.
- IPv6 `HTTPUpdate` uses bracketed URLs (`http://[addr]:port/...`); `HTTPClient` parses RFC 3986 IPv6 literals.
- `Update` API tests are transport-agnostic (no network).
- Host bind address overrides: `OTA_HOST_IP` (IPv4), `OTA_HOST_IPV6` (IPv6).
- `ArduinoOTA.setRebootOnSuccess(false)` and `httpUpdate.rebootOnUpdate(false)` keep the DUT from rebooting mid-suite.
- Signed OTA verification is covered by the separate `signed_ota/` test suite.
