# ESP-NOW Validation Test

Comprehensive validation of the ESP-NOW Arduino API covering broadcast, unicast, encrypted communication, peer management, and lifecycle operations. This is a **multi-DUT** test with 10 phases, each synchronized via serial handshaking.

## Architecture

```
 ┌──────────────┐        ESP-NOW            ┌──────────────┐
 │    Master    │◄────── wireless ─────────►│    Slave     │
 │  (device0)   │                           │  (device1)   │
 └──────┬───────┘                           └──────┬───────┘
        │ serial                                   │ serial
        ▼                                          ▼
 ┌─────────────────────────────────────────────────────────┐
 │                 pytest (test_esp_now.py)                │
 └─────────────────────────────────────────────────────────┘
```

## Test Cases

| Phase | Description |
|---|---|
| 0 | Startup synchronization — both DUTs wait for `START` before proceeding |
| 1 | Init, pre-begin edge cases (`getVersion`/`getMaxDataLen`/`getTotalPeerCount` return -1), master broadcast send, slave `onNewPeer` callback |
| 2 | Slave broadcast send + `recv_bcast` flag verification on master |
| 3 | Unicast master → slave |
| 4 | Unicast slave → master |
| 5 | Peer management: `getTotalPeerCount`, `getEncryptedPeerCount`, `getChannel`, `getInterface`, `isEncrypted`, `operator bool`, `addr()` getter, `addr()` setter edge case (must fail while added), `setRate()` edge case (must fail while added) |
| 5→6 | `setKey()` enables encryption; `isEncrypted` and encrypted peer count verified |
| 6 | Encrypted unicast master → slave |
| 7 | Maximum-length payload (250 or 1470 bytes depending on ESP-NOW version) with byte pattern verification |
| 8 | Peer `remove()` / re-add: `operator bool` false after remove, peer count changes, send after re-add |
| 9 | `end()` / `begin()` lifecycle + `ESP_NOW.write()` broadcast to all registered peers |

## Requirements

- **Hardware**: Two boards with Wi-Fi support (e.g. ESP32, ESP32-S3, ESP32-C6)
- **Wokwi/QEMU**: Not supported (multi-device test)
- **CI Runner**: `two_duts`
- **SoC Config**: `CONFIG_SOC_WIFI_SUPPORTED=y`

## Serial Protocol

1. pytest sends `START` to synchronize both DUTs at phase 0
2. Both DUTs print their MAC address; pytest cross-exchanges MACs between devices
3. For each phase, both DUTs print `Ready for phase N`; pytest sends `START` to slave first, then master (slave-first avoids race conditions)
4. Each phase outputs pass/fail indicators via serial log messages

## Notes

- The slave always receives `START` before the master to avoid race conditions where the master sends before the slave is ready.
- PMK and LMK are hardcoded 16-byte strings for test reproducibility.
- All devices use Wi-Fi channel 1 in STA mode (no AP or internet connection needed).
