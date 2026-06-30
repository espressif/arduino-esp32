# WebServer Validation Test

Validates the `WebServer::send(code, content_type, Stream&)` overload by running a softAP-based HTTP server on one device and an HTTP client on another. This is a **multi-DUT** test that verifies stream-based response sending with text, binary, and empty payloads.

## Architecture

```
 ┌──────────────┐      Wi-Fi + HTTP         ┌──────────────┐
 │   Server     │◄──── GET requests ────────│    Client    │
 │  (device0)   │    (softAP + WebServer)   │  (device1)   │
 └──────┬───────┘                           └──────┬───────┘
        │ serial                                   │ serial
        ▼                                          ▼
 ┌─────────────────────────────────────────────────────────┐
 │               pytest (test_webserver.py)                │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

| Test Function | Description |
|---|---|
| `stream_text` | Stream text data via `send(200, "text/plain", stream)`, verify Content-Length and body |
| `stream_binary` | Stream binary data (0xDEADBEEF), verify Content-Length and byte content |
| `stream_explicit_len` | Stream with explicit content length parameter, verify Content-Length header |
| `stream_empty` | Stream empty data, verify server returns HTTP 204 with empty body |
| `string` | Regression test for `send(200, "text/plain", "OK")` string overload |

## Requirements

- **Hardware**: Two boards with Wi-Fi support (e.g. ESP32)
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`

## Serial Protocol

1. Both devices print `Device ready for Wi-Fi credentials`
2. pytest generates unique SSID/password, sends to server first
3. Server starts softAP and WebServer, prints its IP
4. pytest sends SSID, password, and server IP to client
5. Client connects to AP, then runs all HTTP test cases sequentially
6. Client prints `PASS`/`FAIL` for each test and `All tests passed` at the end

## Notes

- The server uses an in-memory `TestStream` class (simulating a `File`) to test the Stream-based `send()` overload.
- The client uses raw `WiFiClient` connections (not `HTTPClient`) to inspect response headers directly.
