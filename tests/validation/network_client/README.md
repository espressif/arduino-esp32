# NetworkClient Validation Test

Validates the `NetworkClient` API using localhost TCP loopback sockets. No Wi-Fi or external network is required.

## Test Cases

| Test Function | API coverage |
|---|---|
| `test_network_client_default_disconnected` | Default ctor, `connected()`, `operator bool`, `fd()`, `available()`, `read()`, `write()`, `setSSE()` / `isSSE()`, `flush()` |
| `test_network_client_fd_constructor_and_stop` | `NetworkClient(int fd)`, `connected()`, `fd()`, `stop()` |
| `test_network_client_available_read_peek_clear` | `available()`, `peek()`, `read()`, `read(buf)`, `clear()` |
| `test_network_client_read_bytes_large_payload` | `readBytes()` across multiple RX buffer fills (>1436 bytes) |
| `test_network_client_write_variants` | `write(uint8_t)`, `write(buf)`, `write_P()` |
| `test_network_client_write_stream` | `write(Stream&)`, `write(Stream&, length)` |
| `test_network_client_connect_loopback_ip` | `connect(IPAddress, port)`, `setConnectionTimeout()`, `write()`, `readBytes()` |
| `test_network_client_connect_loopback_host` | `connect(host, port, timeout)` via `Network.hostByName()` literal parse |
| `test_network_client_connect_timeout` | `connect()` failure on closed loopback ports |
| `test_network_client_socket_options` | `setNoDelay()` / `getNoDelay()`, `setOption()` / `getOption()`, `setSocketOption()`, `setConnectionTimeout()` |
| `test_network_client_endpoints` | `remoteIP()`, `remotePort()`, `localIP()`, `localPort()` (+ `int fd` overloads) |
| `test_network_client_operator_compare` | `operator!=` between distinct clients |
| `test_network_client_peer_shutdown` | `connected()` after peer closes the socket |

## Requirements

- **Hardware**: Any target with lwIP (loopback TCP)
- **Wokwi**: Supported
- **CI Runner**: Standard hardware runner (no `wifi_router`)

## Notes

- `esp_netif_init()` runs in `setUp()` before each test.
- Hostname connect uses `"127.0.0.1"`, which `Network.hostByName()` resolves without DNS.
