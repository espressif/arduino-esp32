# Wi-Fi Validation Test

Validates Wi-Fi STA and AP functionality: connection, IP/SSID/MAC/RSSI/channel/BSSID/gateway/DNS verification, hostname, disconnect/reconnect, auto-reconnect, network scan, static IP, soft AP mode, event callbacks, and mode switching.

## Test Cases

| Test Function | Description |
|---|---|
| `test_sta_connect` | Connect to AP, verify `WL_CONNECTED` status |
| `test_sta_ip_valid` | Verify assigned IP is non-zero |
| `test_sta_ssid_matches` | Verify connected SSID matches target |
| `test_sta_mac_address` | Verify MAC format (17 chars, 5 colons) |
| `test_sta_rssi` | Verify RSSI is between -100 and 0 dBm |
| `test_sta_channel` | Verify channel is 1-14 |
| `test_sta_bssid` | Verify BSSID string format |
| `test_sta_gateway` | Verify gateway IP is non-zero |
| `test_sta_dns` | Verify DNS server IP is non-zero |
| `test_sta_hostname` | Set hostname, verify it persists after connect |
| `test_sta_disconnect_reconnect` | Disconnect then reconnect successfully |
| `test_sta_auto_reconnect` | Set/get auto-reconnect flag |
| `test_scan_networks` | Scan and find target SSID in results |
| `test_static_ip` | Configure static IP, verify, restore DHCP |
| `test_soft_ap` | Start soft AP, verify IP and SSID, check station count |
| `test_event_fires_on_connect` | Register GOT_IP event handler, verify it fires on connect |
| `test_mode_switching` | Switch between STA, AP, AP_STA, OFF modes |

## Requirements

- **Hardware**: Any ESP32 with Wi-Fi
- **Wokwi**: Supported (uses `Wokwi-GUEST` network)
- **CI Runner**: `wifi_router` (for hardware)

## Serial Protocol

1. Device prints `WIFI_READY`
2. Device prints `Send SSID:` — pytest sends Wi-Fi SSID
3. Device prints `Send Password:` — pytest sends Wi-Fi password
4. Unity test output follows

## Notes

- Wi-Fi credentials are provided by the `wifi_ssid` / `wifi_pass` pytest fixtures.
- `test_event_fires_on_connect` replaced the old `test_event_registration` which only verified registration without triggering the event.
- Static IP test uses `192.168.4.100` — may not work on all network topologies.
