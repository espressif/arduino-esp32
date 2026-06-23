# BLE Validation Test

Validates BLE secure connection between a server and client using Numeric Comparison pairing, characteristic read/write operations, and IRK (Identity Resolving Key) retrieval. This is a **multi-DUT** test supporting both Bluedroid and NimBLE stacks.

## Architecture

```
 ┌──────────────┐          BLE              ┌──────────────┐
 │    Server    │◄──── advertising ─────────│    Client    │
 │  (device0)   │     scan/connect          │  (device1)   │
 └──────┬───────┘                           └──────┬───────┘
        │ serial                                   │ serial
        ▼                                          ▼
 ┌─────────────────────────────────────────────────────────┐
 │                    pytest (test_ble.py)                 │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

### Server (`server/server.ino`)

| Test Function | Description |
|---|---|
| Server initialization | Start BLE server with Numeric Comparison security, advertise with a unique name |
| Insecure characteristic | Serve read/write on an unprotected characteristic |
| Secure characteristic | Serve read/write requiring MITM authentication |
| Numeric Comparison PIN | Display and auto-confirm pairing PIN |
| IRK retrieval | Retrieve and print the peer's Identity Resolving Key after authentication |

### Client (`client/client.ino`)

| Test Function | Description |
|---|---|
| Scan and connect | Scan for the target server by name and service UUID, then connect |
| Insecure characteristic read | Read unprotected characteristic value without authentication |
| Secure characteristic read | Read protected characteristic, triggering on-demand authentication |
| Numeric Comparison PIN | Display and auto-confirm pairing PIN (must match server) |
| IRK retrieval | Retrieve and print the peer's Identity Resolving Key after authentication |
| Write/read operations | Write and read back values on both secure and insecure characteristics |

## Requirements

- **Hardware**: Two boards with BLE support (e.g. ESP32, ESP32-S3)
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_SOC_BLE_SUPPORTED=y`

## Serial Protocol

1. Both devices print `Device ready for server name`
2. pytest generates a unique server name and sends it to both devices
3. Server begins advertising; client scans for the server name
4. Client connects, discovers service/characteristics
5. Client reads insecure characteristic (no auth)
6. Client reads secure characteristic (triggers Numeric Comparison)
7. Both devices display and confirm the same PIN
8. Authentication completes; both devices retrieve peer IRK
9. Client performs write/read on both characteristics

## Notes

- NVS is erased at startup to ensure fresh pairing on every run.
- Authentication is triggered on-demand (when reading the secure characteristic) rather than on connect, ensuring consistent ordering across Bluedroid and NimBLE.
- The test verifies PIN match between server and client via assertion.
