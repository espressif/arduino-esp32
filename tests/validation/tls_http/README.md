# TLS / HTTP Client Validation Test

Validates `NetworkClientSecure` TLS connections (CA cert, insecure mode, send/receive) and `HTTPClient` operations (GET, POST, custom headers, timeout, HTTPS) against postman-echo.com.

## Test Cases

| Test Function | Description |
|---|---|
| `test_tls_with_ca` | TLS handshake with CA certificate to postman-echo.com:443 |
| `test_tls_insecure` | TLS connect with `setInsecure()` (skip cert verification) |
| `test_tls_send_receive` | Send raw HTTP GET over TLS, verify 200 response |
| `test_http_get` | `HTTPClient` HTTPS GET via CA cert, verify 200 and body content |
| `test_http_post` | `HTTPClient` HTTPS POST with JSON payload, verify echoed body |
| `test_http_custom_header` | `HTTPClient` HTTPS GET with `X-Custom-Test` header, verify echoed |
| `test_https_get` | `HTTPClient` HTTPS GET via `NetworkClientSecure` with CA cert (status only) |
| `test_http_timeout` | `HTTPClient` to unreachable IP (192.0.2.1), verify timeout error |

## Requirements

- **Hardware**: Any ESP32 variant with Wi-Fi support (except ESP32-C6 — insufficient RAM)
- **Wokwi/QEMU**: QEMU not supported
- **CI Runner**: `wifi_router`
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`

## Serial Protocol

1. DUT prints `TLS_HTTP_READY`
2. DUT prints `Send SSID:` → host sends SSID
3. DUT prints `Send Password:` → host sends password
4. DUT prints `SEND_CA_CERT` → host sends PEM lines followed by `CERT_END`
5. DUT prints `GOT_CERT len=<n>`
6. Unity test suite runs

## Notes

- The pytest script fetches the root CA certificate for postman-echo.com at test time and sends it line-by-line over serial.
- The serial RX buffer is set to 4096 bytes to accommodate the CA certificate transfer.
- Internet access is required during the test for connections to postman-echo.com.
