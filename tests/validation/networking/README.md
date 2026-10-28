# Networking Validation Test

Validates core networking APIs over Wi-Fi: DNS resolution, TCP client (connect, send/receive, timeout, large payload), TCP server echo, HTTPClient stream uploads (short reads and writes), UDP (send/receive, multi-packet, remote info, IPv4/IPv6 loopback sends), DNS server (captive portal), and mDNS (hostname and service registration).

## Test Cases

| Test Function | Description |
|---|---|
| `test_dns_resolve` | Resolve `pool.ntp.org` via `Network.hostByName()` |
| `test_dns_resolve_invalid` | Verify resolution fails for non-existent domain |
| `test_tcp_connect` | TCP connect to `httpbin.org:80` |
| `test_tcp_send_receive` | HTTP GET request, verify response starts with `HTTP/1.1` |
| `test_tcp_timeout` | Connect to unreachable IP with 2 s timeout, verify failure |
| `test_tcp_large_payload` | Request 1024-byte response, verify total read > 1024 |
| `test_tcp_server_echo` | Start local server, connect, send/receive echo |
| `test_udp_write_before_begin` | Verify single-byte, buffer, and zero-length writes return zero before initialization |
| `test_udp_write_after_stop` | Verify buffer writes succeed while initialized and single-byte and buffer writes return zero after stopping |
| `test_http_stream_full_read` | Upload from a stream that supplies each requested chunk |
| `test_http_stream_short_read` | Retry an upload when each stream read supplies only part of the requested data |
| `test_http_stream_short_read_short_write` | Verify short stream reads and short transport writes are accounted for independently |
| `test_http_stream_full_read_short_write` | Retry short transport writes after a full stream read |
| `test_http_stream_content_length_limit` | Upload only the requested portion of a longer stream |
| `test_http_stream_failed_retry` | Report payload failure when the transport cannot finish a partial write |
| `test_udp_send_receive` | UDP loopback: send/receive on localhost |
| `test_udp_multi_packet` | Send 5 UDP packets, verify at least 4 received |
| `test_udp_remote_info` | Verify remote IP and port after receiving UDP packet |
| `test_udp_ipv4_without_local_bind` | A fresh sender sends twice to an IPv4 loopback receiver without `begin()` |
| `test_udp_ipv6_without_local_bind` | A fresh sender sends twice to an IPv6 loopback receiver without `begin()` |
| `test_udp_ipv4_with_local_bind` | Verify an explicitly bound IPv4 sender still sends successfully |
| `test_udp_ipv6_with_local_bind` | Verify an explicitly bound IPv6 sender still sends successfully |
| `test_udp_socket_family_after_stop` | Send IPv4 → IPv6 → IPv4 on one sender with `stop()` between families |
| `test_dns_server_captive` | Start DNSServer on softAP, verify `start()` succeeds |
| `test_mdns_begin` | Start mDNS with hostname |
| `test_mdns_service` | Register HTTP and custom UDP services via mDNS |

## Requirements

- **Hardware**: Any ESP32 with Wi-Fi
- **Wokwi**: Supported (uses `Wokwi-GUEST` network)
- **CI Runner**: `wifi_router` (for hardware)
- UDP loopback checks are ignored when `CONFIG_LWIP_NETIF_LOOPBACK` is disabled; IPv6 checks are ignored when IPv6 is disabled.

## Serial Protocol

1. Device prints `NET_READY`
2. Device prints `Send SSID:` — pytest sends Wi-Fi SSID
3. Device prints `Send Password:` — pytest sends Wi-Fi password
4. Unity test output follows

## Notes

- Wi-Fi credentials are provided by the `wifi_ssid` / `wifi_pass` pytest fixtures.
- TCP tests use `httpbin.org` (requires internet on the CI runner).
- UDP and server tests use localhost loopback (no external dependency).
