# Matter On-Network Wi-Fi Example

Commission a Matter On/Off Light over Wi-Fi **without CHIPoBLE**. The sketch connects with `ssid` / `password` first.

This is the on-network half of the Wi-Fi pair. The other half is [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi) (BLE on, hub sends SSID/password).

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`) in this sketch.** Matter owns NimBLE. `selectNetwork(WIFI, true)` turns CHIPoBLE off and releases BLE RAM; it does not hand the radio to Arduino BLE.

## What it does

- Calls `Matter.selectNetwork(MATTER_NETWORK_WIFI, true)` before any accessory `begin()` (CHIPoBLE off)
- Connects Wi-Fi first, then starts Matter
- Prints pairing codes for on-network commissioning
- Prints `ESP.getFreeHeap()` before/after `begin()`. `onBLEMemoryReleased()` is registered but usually does not run here (`selectNetwork(WIFI, true)` turns CHIPoBLE off before `begin()`). See [MatterCHIPoBLERelease](../MatterCHIPoBLERelease) for allocate-from-`loop()` after reclaim.

## Supported targets

| SoC      | This sketch             | CHIPoBLE | Also in prebuild       |
| -------- | ----------------------- | -------- | ---------------------- |
| ESP32    | Wi-Fi (SSID in sketch)  | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Wi-Fi (SSID in sketch)  | Off      | Ethernet (SPI)         |
| ESP32-S3 | Wi-Fi (SSID in sketch)  | Off      | Ethernet (SPI)         |
| ESP32-C3 | Wi-Fi (SSID in sketch)  | Off      | Ethernet (SPI)         |
| ESP32-C5 | Wi-Fi (SSID in sketch)  | Off      | Ethernet (SPI)         |
| ESP32-C6 | Wi-Fi (SSID in sketch)  | Off      | Thread, Ethernet (SPI) |
| ESP32-H2 | Does not run (no Wi-Fi) | —        | Thread, Ethernet (SPI) |

This sketch calls `Matter.selectNetwork(MATTER_NETWORK_WIFI, true)` and `WiFi.begin(ssid, password)`. On the Arduino IDE prebuild, ESP32 / ESP32-S2 already have CHIPoBLE off.

- ESP32-H2: use [MatterOnNetworkThread](../MatterOnNetworkThread) or [MatterCHIPoBLEThread](../MatterCHIPoBLEThread).
- ESP32-C6 also has Thread; this sketch keeps Wi-Fi. For Thread see [MatterOnNetworkThread](../MatterOnNetworkThread).
- Wi-Fi + CHIPoBLE (hub sends SSID): [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi).
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../MatterOnNetworkEthernet).

Change the path with `Matter.selectNetwork()` before any accessory `begin()`. Do not also call `setBLECommissioningEnabled()`.

## Setup

1. Set `ssid` and `password` in the sketch.
2. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
3. Serial Monitor 115200.

Long-press BOOT (>5 s) to decommission the node (`Matter.decommission()`). This removes fabrics; it is not a full flash erase.

## Related

- [Matter overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi) — BLE on, no SSID (hub sends credentials)
- [MatterOnNetworkEthernet](../MatterOnNetworkEthernet) — same on-network pattern for Ethernet
- [MatterOnNetworkThread](../MatterOnNetworkThread) — same on-network pattern for Thread
- [MatterCHIPoBLERelease](../MatterCHIPoBLERelease) — CHIPoBLE then BLE RAM reclaim
