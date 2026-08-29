# Matter On-Network Wi-Fi Example

Commission over an existing Wi-Fi connection when CHIPoBLE is off or unused, including CHIPoBLE SoCs (ESP32-C3/C6/S3) and original ESP32 if you turn CHIPoBLE off.

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`) in this sketch.** Matter owns NimBLE. `setBLECommissioningEnabled(false)` releases BLE RAM; it does not hand the radio to Arduino BLE.

## What it does

- Calls `Matter.setBLECommissioningEnabled(false)` before `Matter.begin()`
- Connects Wi-Fi first, then starts Matter
- Prints pairing codes for on-network commissioning
- Prints `ESP.getFreeHeap()` before/after `begin()` and from `Matter.onBLEMemoryReleased()` when BLE RAM is back
- Does not `malloc` in that callback (CHIP task). See `MatterCHIPoBLERelease` for allocate-from-`loop()`

## Supported targets

Needs Wi-Fi. Not for Thread-only boards (ESP32-C5 / ESP32-H2 in the Arduino Matter prebuild).

| SoC | Notes |
| --- | ----- |
| ESP32 (Arduino IDE) | CHIPoBLE is not in the prebuild (Bluedroid); setter `false` is a no-op |
| ESP32 (IDF component, NimBLE + CHIPoBLE) | CHIPoBLE can be on; this sketch turns it off and frees BLE RAM |
| ESP32-S2 | No Bluetooth hardware; setter `false` is a no-op |
| ESP32-S3 / C3 / C6 | CHIPoBLE is compiled in; this sketch turns it off and frees BLE RAM |

## Setup

1. Set `ssid` and `password` in the sketch.
2. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
3. Serial Monitor 115200.

Long-press BOOT (>5 s) to factory-reset.

## Related

- [Matter overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- Example `MatterCHIPoBLERelease` — keep CHIPoBLE, `onBLEMemoryReleased()`, then `malloc` from `loop()`
