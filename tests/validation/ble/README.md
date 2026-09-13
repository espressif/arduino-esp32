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
| Write-NR characteristic | Serve Write / Write-Without-Response; capture burst payloads in `onWrite()` (issue #12815) |
| Heap integrity | `heap_caps_check_integrity_all()` before and after `BLEDevice::init()` (issue #12821) |
| Numeric Comparison PIN | Display and auto-confirm pairing PIN |
| IRK retrieval | Retrieve and print the peer's Identity Resolving Key after authentication |
| Malformed advertisement data | Advertise deliberately malformed AD structures so the client can prove its parser rejects them |

### Client (`client/client.ino`)

| Test Function | Description |
|---|---|
| Scan and connect | Scan for the target server by name and service UUID, then connect |
| Insecure characteristic read | Read unprotected characteristic value without authentication |
| Secure characteristic read | Read protected characteristic, triggering on-demand authentication |
| Numeric Comparison PIN | Display and auto-confirm pairing PIN (must match server) |
| IRK retrieval | Retrieve and print the peer's Identity Resolving Key after authentication |
| Write/read operations | Write and read back values on both secure and insecure characteristics |
| Write-NR burst | Three back-to-back Write-Without-Response packets (`AA`, `BB`, `CC`) with no delay (issue #12815) |
| Heap integrity | `heap_caps_check_integrity_all()` before and after `BLEDevice::init()` (issue #12821) |
| Advertisement parsing | Verify malformed AD structures are rejected instead of turned into fields (`ADCHK1` / `ADCHK2`) |

## Advertisement Parsing Regression

`BLEAdvertisedDevice::parseAdvertisement` must reject malformed AD structures
rather than read past the valid payload or accept a truncated list. The server
advertises them in two windows, because the terminator and the oversize guard
both stop the parse loop and so each one has to be the last structure in its own
payload to be observable.

| Window | Malformed structure | Expected result |
|---|---|---|
| 1 | 16-bit UUID list with a trailing partial octet | `svc16=0` (list rejected whole) |
| 1 | 128-bit UUID structure with fewer than 16 data bytes | `svcCount=1` (no UUID built from an over-read) |
| 1 | AD length larger than the remaining payload | `mfg=0` (structure rejected) |
| 2 | 32-bit UUID list with a trailing partial group | `svc32=0` (list rejected whole) |
| 2 | Manufacturer data behind a zero-length terminator | `mfg=0` (parsing stops at the terminator) |

Window 2 is entered on demand: pytest sends `ADPHASE2` to the server, waits for
it to re-advertise, then sends `ADPHASE2` to the client to trigger a rescan.

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
10. Client sends a 3-packet Write-Without-Response burst; server must report `AA AA AA`, `BB BB BB`, `CC CC CC`
11. Heap integrity is checked around `BLEDevice::init()` on both devices (and again on the server after the burst)
12. pytest sends `ADPHASE2` to both devices to run the second advertisement parsing window

## Notes

- NVS is erased at startup to ensure fresh pairing on every run.
- Authentication is triggered on-demand (when reading the secure characteristic) rather than on connect, ensuring consistent ordering across Bluedroid and NimBLE.
- The test verifies PIN match between server and client via assertion.
