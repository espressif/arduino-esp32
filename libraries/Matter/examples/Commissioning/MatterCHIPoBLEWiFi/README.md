# Matter CHIPoBLE Wi-Fi Example

Commission a Matter On/Off Light onto Wi-Fi **over CHIPoBLE**. The hub sends the SSID and password. The sketch does **not** call `WiFi.begin()`.

This is the BLE half of the Wi-Fi pair. The other half is [MatterOnNetworkWiFi](../MatterOnNetworkWiFi) (BLE off + credentials in the sketch).

**Do not start Arduino `ESPmDNS`.** **Do not use `BLE.h` / `BLEDevice`.**

## What it does

- `Matter.selectNetwork(MATTER_NETWORK_WIFI)` before any accessory `begin()` (CHIPoBLE stays **on**)
- Starts the On/Off Light and `Matter.begin()`
- Prints pairing codes for BLE commissioning

Same idea as Thread: BLE is the bootstrap when the device has no network credentials yet.

## Supported targets

| SoC      | This sketch                 | CHIPoBLE | Also in prebuild       |
| -------- | --------------------------- | -------- | ---------------------- |
| ESP32    | Does not run (no CHIPoBLE)  | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Does not run (no Bluetooth) | Off      | Ethernet (SPI)         |
| ESP32-S3 | Wi-Fi (hub)                 | On       | Ethernet (SPI)         |
| ESP32-C3 | Wi-Fi (hub)                 | On       | Ethernet (SPI)         |
| ESP32-C5 | Wi-Fi (hub)                 | On       | Ethernet (SPI)         |
| ESP32-C6 | Wi-Fi (hub)                 | On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Does not run (no Wi-Fi)     | On       | Thread, Ethernet (SPI) |

This sketch calls `Matter.selectNetwork(MATTER_NETWORK_WIFI)` and leaves CHIPoBLE **on**. It does not call `WiFi.begin()`.

- ESP32 / ESP32-S2: use [MatterOnNetworkWiFi](../MatterOnNetworkWiFi).
- ESP32-H2: use [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) or [MatterOnNetworkThread](../MatterOnNetworkThread).
- ESP32-C6 also has Thread; this sketch keeps Wi-Fi. For Thread see [MatterCHIPoBLEThread](../MatterCHIPoBLEThread).
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../MatterOnNetworkEthernet).

Change the path with `Matter.selectNetwork()` before any accessory `begin()`. Do not also call `setBLECommissioningEnabled()`.

## Setup

1. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
2. Serial Monitor 115200.
3. Commission with the printed code over BLE.

Long-press BOOT (>5 s) to decommission the node (`Matter.decommission()`). This removes fabrics; it is not a full flash erase.

## Related

- [MatterOnNetworkWiFi](../MatterOnNetworkWiFi) — BLE off, SSID/password in the sketch
- [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) — same BLE path for Thread (hub sends the dataset)
- [MatterCHIPoBLERelease](../MatterCHIPoBLERelease) — CHIPoBLE then BLE RAM reclaim
