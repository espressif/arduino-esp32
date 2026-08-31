# Matter CHIPoBLE Thread Example

Commission a Matter On/Off Light onto Thread **over CHIPoBLE**. The hub sends the Thread dataset. The sketch does **not** configure one.

This is the BLE half of the Thread pair. The other half is [MatterOnNetworkThread](../MatterOnNetworkThread) (BLE off + network key in the sketch).

**Do not start Arduino `ESPmDNS`.** **Do not use `BLE.h` / `BLEDevice`.**

## What it does

- `Matter.selectNetwork(MATTER_NETWORK_THREAD)` before any accessory `begin()` (CHIPoBLE stays **on**)
- Starts the On/Off Light and `Matter.begin()`
- Prints pairing codes for BLE commissioning
- On dual-stack ESP32-C6, Thread Network Commissioning replaces Wi-Fi on endpoint 0

Do not call `OThread.begin()` or `commitDataSet()` here. After the hub finishes, CHIP has the dataset.

Do not type a Wi-Fi password into this example. The hub should offer Thread networks (or send a dataset) after BLE pairing.

## Supported targets

| SoC      | This sketch                           | CHIPoBLE | Also in prebuild       |
| -------- | ------------------------------------- | -------- | ---------------------- |
| ESP32    | Does not run (no Thread)              | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Does not run (no Thread)              | Off      | Ethernet (SPI)         |
| ESP32-S3 | Does not run (no Thread)              | On       | Ethernet (SPI)         |
| ESP32-C3 | Does not run (no Thread)              | On       | Ethernet (SPI)         |
| ESP32-C5 | Does not run (Thread not in prebuild) | On       | Ethernet (SPI)         |
| ESP32-C6 | Thread (hub)                          | On       | Wi-Fi, Ethernet (SPI)  |
| ESP32-H2 | Thread (hub)                          | On       | Ethernet (SPI)         |

This sketch calls `Matter.selectNetwork(MATTER_NETWORK_THREAD)` and leaves CHIPoBLE **on**. The hub sends the Thread dataset. Do not call `OThread.begin()` here.

- Arduino Matter prebuild Thread: **ESP32-C6** and **ESP32-H2**. ESP32-C5 reports Thread unsupported until Matter-over-Thread is in that prebuild.
- ESP32-C6 also has Wi-Fi; this sketch selects Thread. For Wi-Fi see [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi).
- Thread on-network (CHIPoBLE off): [MatterOnNetworkThread](../MatterOnNetworkThread).
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../MatterOnNetworkEthernet).

Change the path with `Matter.selectNetwork()` before any accessory `begin()`. Do not also call `setBLECommissioningEnabled()`.

## Setup

1. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
2. Serial Monitor 115200.
3. Commission with the printed code over BLE.

Long-press BOOT (>5 s) to decommission the node (`Matter.decommission()`). This removes fabrics; it is not a full flash erase.

## Related

- [MatterOnNetworkThread](../MatterOnNetworkThread) — BLE off, network key in the sketch
- [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi) — same BLE path for Wi-Fi (hub sends SSID/password)
- [MatterCHIPoBLERelease](../MatterCHIPoBLERelease) — CHIPoBLE then BLE RAM reclaim
