# Wi-Fi AP Validation Test

Validates Wi-Fi Access Point creation and station connection using the Arduino Wi-Fi API. This is a **multi-DUT** test: device0 creates a softAP and device1 connects to it as a station.

## Architecture

```
 ┌──────────────┐         Wi-Fi             ┌──────────────┐
 │  Access Point│◄──── association ─────────│    Client    │
 │  (device0)   │                           │  (device1)   │
 └──────┬───────┘                           └──────┬───────┘
        │ serial                                   │ serial
        ▼                                          ▼
 ┌─────────────────────────────────────────────────────────┐
 │                 pytest (test_wifi_ap.py)                │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

| Test Function | Description |
|---|---|
| AP creation | Start softAP with unique SSID and password, verify valid IP |
| Client scan | Client scans for networks and finds the AP's SSID |
| Client connect | Client connects to the AP, receives a valid IP address |
| Station count | AP reports 1 connected station |
| Connection status | Client reports `WL_CONNECTED` status (status code 3) |

## Requirements

- **Hardware**: Two boards with Wi-Fi support (e.g. ESP32, ESP32-S3, ESP32-C6)
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`
- **FQBN**: Multiple configurations tested with PSRAM enabled/disabled

## Serial Protocol

1. Both devices print `Device ready for Wi-Fi credentials`
2. pytest generates a unique SSID and password, sends them to both devices
3. AP starts and prints its IP address
4. Client scans, connects, and prints its IP address
5. AP confirms station count; client confirms connected status

## Notes

- SSID and password are unique per test run (using CI job ID or random suffix) to avoid interference with other APs.
- Multiple FQBN configurations are tested for ESP32, ESP32-S2, and ESP32-S3, covering PSRAM enabled/disabled variants.
