# WebServer Validation Test

Validates the `WebServer::send(code, content_type, Stream&)` overload and a set of HTTP request-parsing robustness properties by running a softAP-based HTTP server on one device and an HTTP client on another. This is a **multi-DUT** test: the server exposes endpoints configured the same way as the upstream `WebServer` examples, and the client crafts both well-formed and malformed raw HTTP requests to exercise the server's parser.

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

### `send(Stream&)` overload

| Test Function | Description |
|---|---|
| `stream_text` | Stream text data via `send(200, "text/plain", stream)`, verify Content-Length and body |
| `stream_binary` | Stream binary data (0xDEADBEEF), verify Content-Length and byte content |
| `stream_explicit_len` | Stream with explicit content length parameter, verify Content-Length header |
| `stream_empty` | Stream empty data, verify server returns HTTP 204 with empty body |
| `string` | Regression test for `send(200, "text/plain", "OK")` string overload |

### Request-parsing robustness (security regression checks)

Each case first confirms the endpoint behaves correctly for a well-formed request, then sends a malformed/hostile request and verifies the server neither leaks data, bypasses a check, nor stops responding. After the destructive checks the client re-probes `/alive` to confirm the server task survived.

| Test Function | Property verified |
|---|---|
| `auth_bypass` | A bare `Authorization: <username>` header must not bypass the configured plaintext password (only `Basic`/`Digest` are accepted). |
| `path_traversal` | `serveStatic()` must not serve files outside its configured root via `..` dot segments. |
| `arg_poison` | A completed field from an aborted, unauthenticated multipart request must not shadow a later request's named arguments. |
| `arg_flood` | An oversized query string (many `&` separators) must not trigger an unbounded allocation that resets the device. |
| `upload_null_deref` | A non-multipart POST to an upload route must not dereference a null upload object. |
| `regex_stack` | A long path against a `UriRegex` route with unbounded repetition must not exhaust the task stack. |
| `regex_backtrack` | A short, non-matching path against a `UriRegex` route with nested quantifiers (`(a+)+`) must not block the server task in exponential backtracking. |
| `multipart_eof` | A multipart body truncated right after the initial boundary must not spin the parser in an unbounded loop. |
| `long_line` | A request line or header with no CRLF must be refused (`414`/`431`) instead of being accumulated until the heap is exhausted, while long-but-legal headers are still accepted. |
| `slow_line` | A client trickling bytes into a header line it never terminates must be dropped on a deadline (`408`), not kept alive one byte at a time ([#12788](https://github.com/espressif/arduino-esp32/issues/12788)). |
| `slow_headers` | A client sending an endless stream of complete but tiny headers must be dropped by the header-phase deadline, which per-line deadlines cannot catch. |

### Compatibility (limits must not refuse legitimate requests)

The checks above introduce bounds on request size and shape. These cases pin down the request shapes that must keep working, so a future tightening cannot silently break applications.

| Test Function | Property verified |
|---|---|
| `static_root` | `serveStatic()` whose filesystem root is `/` still serves files (the mapping used by the `WebServer` example). |
| `raw_body` | A non-multipart body on a route registered with an upload-style callback still streams to that callback via `server.raw()`. |
| `many_args` | A urlencoded form with 100 fields is parsed in full, not silently reduced to zero arguments. |
| `long_uri` | A ~900 byte query string is served normally, while an over-long request-target is answered with `414` instead of a dropped connection. |
| `regex_route` | A `UriRegex` route still matches a normal-length path, including one with nested quantifiers. |

The limits are compile-time configurable: `WEBSERVER_MAX_URI_LEN`, `WEBSERVER_MAX_QUERY_ARGS`, `WEBSERVER_MAX_LINE_LEN`, `WEBSERVER_MAX_POST_ARG_LEN`, `WEBSERVER_MAX_MULTIPART_SKIP_LINES`, `WEBSERVER_MAX_LINE_WAIT`, `WEBSERVER_MAX_HEADER_WAIT`, `WEBSERVER_MAX_REGEX_URI_LEN` and `WEBSERVER_MAX_BACKREF_REGEX_URI_LEN`.

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
