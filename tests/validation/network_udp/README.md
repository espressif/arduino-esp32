# NetworkUDP Socket Family Validation

Uses UDP loopback receivers bound to ephemeral ports to check `NetworkUDP` sends.
No Wi-Fi connection, external server, or DNS resolution is required.

| Test Function | Description |
|---|---|
| `test_udp_ipv4_without_local_bind` | A fresh sender reaches an IPv4 receiver without calling `begin()` |
| `test_udp_ipv6_without_local_bind` | A fresh sender reaches an IPv6 receiver without calling `begin()` |
| `test_udp_ipv4_with_local_bind` | An explicitly bound IPv4 sender continues to send successfully |
| `test_udp_ipv6_with_local_bind` | An explicitly bound IPv6 sender continues to send successfully |
| `test_udp_socket_family_after_stop` | One sender sends via IPv4, IPv6, and IPv4 again with `stop()` between families |

Each receiver checks two datagrams for their exact binary payload, source address
family, and a nonzero source port that stays the same across repeated sends.
The receive timeout is three seconds. IPv6 cases are skipped when `LWIP_IPV6` is
disabled. Loopback support must be enabled in the SDK configuration.
