# Ethernet Validation Test

Validates Ethernet connectivity on boards with a physical PHY (RMII or SPI). Tests cover link detection, DHCP IP acquisition, MAC address format, link speed, gateway/subnet/DNS validation, DNS resolution, TCP connectivity, and static IP configuration.

## Test Cases

| Test Function | Description |
|---|---|
| `test_eth_link_up` | Wait for link up event and DHCP IP within 30 s timeout |
| `test_eth_ip_valid` | Verify assigned IP is non-zero |
| `test_eth_mac_address` | Verify MAC address format (17 chars, 5 colons) |
| `test_eth_link_speed` | Verify link speed is 10 or 100 Mbps |
| `test_eth_gateway` | Verify gateway IP is non-zero |
| `test_eth_subnet` | Verify subnet mask is valid (first octet 255) |
| `test_eth_dns_server` | Verify DNS server IP is assigned |
| `test_eth_dns_resolution` | Resolve `pool.ntp.org` via DNS |
| `test_eth_tcp_gateway` | TCP connect to gateway:80 or DNS:53 |
| `test_eth_static_ip` | Configure static IP, verify, then restore DHCP |

## Requirements

- **Hardware**: ESP32 board with Ethernet PHY (e.g., LAN8720 RMII or SPI-based)
- **Wokwi/QEMU**: Not supported
- **CI Runner**: `ethernet`

## Notes

- Uses event-driven link detection (`ARDUINO_EVENT_ETH_CONNECTED` / `ARDUINO_EVENT_ETH_GOT_IP`).
- TCP connectivity test uses gateway or DNS server (no external service dependency).
- Static IP test uses `192.168.200.250` then restores DHCP.
