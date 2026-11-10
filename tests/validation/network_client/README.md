# NetworkClient Validation Test

Validates the `NetworkClient` API using localhost TCP loopback sockets. No Wi-Fi or external network is required.

Also validates `IPAddress` string parsing: IPv4, IPv6, scoped IPv6, clearing a
previous zone, initialized string constructors, and rejection of invalid or null
input. Failed parses do not promise to preserve the previous address. These
cases run through the existing NetworkClient test driver so they
are available to the CI runner that checks out test scripts from the base branch.

## Test Cases

| Test Function | API coverage |
|---|---|
| `test_network_client_default_disconnected` | Default ctor, `connected()`, `operator bool`, `fd()`, `available()`, `read()`, `write()`, `setSSE()` / `isSSE()`, `flush()` |
| `test_network_client_fd_constructor_and_stop` | `NetworkClient(int fd)`, `connected()`, `fd()`, `stop()` |
| `test_network_client_available_read_peek_clear` | `available()`, `peek()`, `read()`, `read(buf)`, `clear()` |
| `test_network_client_read_bytes_large_payload` | `readBytes()` across multiple RX buffer fills (>1436 bytes) |
| `test_network_client_write_variants` | `write(uint8_t)`, `write(buf)`, `write_P()` |
| `test_network_client_write_stream` | `write(Stream&)`, `write(Stream&, length)` |
| `test_network_client_write_stream_stops_on_short_write` | Both stream overloads stop consuming after a partial transport write and report the accepted byte count |
| `test_network_client_write_stream_stops_on_zero_write` | Both stream overloads stop consuming after a failed transport write |
| `test_network_client_write_stream_stops_when_source_stalls` | Unbounded stream write exits when `available()` is positive but `readBytes()` returns zero |
| `test_network_client_connect_loopback_ip` | `connect(IPAddress, port)`, `setConnectionTimeout()`, `write()`, `readBytes()` |
| `test_network_client_connect_loopback_host` | `connect(host, port, timeout)` via `Network.hostByName()` literal parse |
| `test_network_client_connect_timeout` | `connect()` failure on closed loopback ports |
| `test_network_client_socket_options` | `setNoDelay()` / `getNoDelay()`, `setOption()` / `getOption()`, `setSocketOption()`, `setConnectionTimeout()` |
| `test_network_client_endpoints` | `remoteIP()`, `remotePort()`, `localIP()`, `localPort()` (+ `int fd` overloads) |
| `test_network_client_operator_compare` | `operator!=` between distinct clients |
| `test_network_client_peer_shutdown` | `connected()` after peer closes the socket |
| `test_network_client_peer_fin_reports_disconnected` | `connected()` becomes false after an orderly FIN once unread data is drained, but stays true while data remains |
| `test_network_client_peer_close_reports_disconnected` | `connected()` becomes false after the peer calls `close()` |
| `test_network_client_peer_half_close_allows_reply` | `read()` reports EOF after a half-close and the client can still write the reply |
| `test_network_client_peer_half_close_allows_reply_after_probe` | A half-closed socket remains writable after `connected()` has reported the FIN |
| `test_network_server_accept_roundtrip` | `NetworkServer::hasClient()` / `accept()` and a bidirectional exchange on the accepted socket |
| `test_network_server_accept_closes_socket_on_option_failure` | `accept()` closes the descriptor when its `setsockopt()` calls fail |

## Requirements

- **Hardware**: Any target with lwIP (loopback TCP)
- **Wokwi**: Supported
- **CI Runner**: Standard hardware runner (no `wifi_router`)

## Notes

- `esp_netif_init()` runs in `setUp()` before each test.
- Hostname connect uses `"127.0.0.1"`, which `Network.hostByName()` resolves without DNS.
