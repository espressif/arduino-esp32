# Matter On-Network Thread Example

Commission a Matter On/Off Light over Thread **without CHIPoBLE**. After `Matter.begin()`, the sketch attaches to CHIP's Thread stack and commits the **border router network key**.

This is the on-network half of the Thread pair. The other half is [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) (BLE on, hub sends the dataset).

**Alexa / most consumer apps discover Matter devices over BLE.** With CHIPoBLE off they will not see this node. Use [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) for that. This example is for a controller that browses `_matterc._udp` on the Thread mesh (via the border router SRP / DNS-SD proxy).

**Do not start Arduino `ESPmDNS`.** CHIP owns the mDNS responder.

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`) in this sketch.**

**Do not call `OThread.begin()` before `Matter.begin()`.** CHIP starts the Thread stack; `OThread.begin()` afterwards attaches to it (`isAttachedToExternalStack()`). Calling `begin()` first would start a second stack.

**Do not call `OThreadDNSSD.begin()`.** CHIP owns the Thread SRP client (`_matterc._udp`). Arduino DNS-SD `begin()` is refused while attached.

## What it does

- Calls `Matter.selectNetwork(MATTER_NETWORK_THREAD, true)` before any accessory `begin()` (CHIPoBLE off)
- Starts the On/Off Light and `Matter.begin()`
- Attaches `OThread` to CHIP's instance, commits a `DataSet` that holds only the network key, then `start()` / `waitForAttach()` / `waitForNetwork()`
- Prints on-network pairing codes. CHIP publishes `_matterc._udp` through Thread SRP after the border router SRP server is found (`Matter DNS-SD initialized (Thread SRP ready)` in the log). That is not the same as attach: on ESP32-C6 an earlier `Matter DNS-SD initialized` is only Wi-Fi mDNS. The library retries advertise for about 24 s after Thread IPv6 / attach. If the app still does not see the node, the border router is not proxying SRP to LAN mDNS — use [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) for consumer apps.

Thread Network Commissioning is on endpoint 0 (ESP32-C6 replaces the prebuild Wi-Fi cluster on the root; ESP32-H2 and ESP32-C5 Thread menu are already Thread-only).

## Network key

Set `threadNetworkKey` in the sketch to the **same 16-byte network key** as the Thread border router (`ot-ctl dataset networkkey`). This example does not set network name, channel, PAN ID, or Extended PAN ID.

The sketch commits that key on every boot (it does not skip commit when NVS already has a dataset).

## Supported targets

| SoC      | This sketch                          | CHIPoBLE | Also in prebuild       |
| -------- | ------------------------------------ | -------- | ---------------------- |
| ESP32    | Does not run (no Thread)             | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Does not run (no Thread)             | Off      | Ethernet (SPI)         |
| ESP32-S3 | Does not run (no Thread)             | On       | Ethernet (SPI)         |
| ESP32-C3 | Does not run (no Thread)             | On       | Ethernet (SPI)         |
| ESP32-C5 | Thread (**Matter Network → Thread**) | Off      | Wi-Fi, Ethernet (SPI)  |
| ESP32-C6 | Thread (key in sketch)               | Off      | Wi-Fi, Ethernet (SPI)  |
| ESP32-H2 | Thread (key in sketch)               | Off      | Ethernet (SPI)         |

This sketch calls `Matter.selectNetwork(MATTER_NETWORK_THREAD, true)` (CHIPoBLE **off**), then after `Matter.begin()` attaches `OThread` and commits the border-router network key.

- Arduino Matter prebuild Thread: **ESP32-C6**, **ESP32-H2**, and **ESP32-C5** with **Tools → Matter Network → Thread**.
- Most consumer apps discover over BLE. For that path use [MatterCHIPoBLEThread](../MatterCHIPoBLEThread).
- ESP32-C6 also has Wi-Fi; this sketch selects Thread. For Wi-Fi see [MatterOnNetworkWiFi](../MatterOnNetworkWiFi).
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../MatterOnNetworkEthernet).

Change the path with `Matter.selectNetwork()` before any accessory `begin()`. Do not also call `setBLECommissioningEnabled()`.

## Setup

1. Copy the border router network key into `threadNetworkKey`.
2. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
3. Serial Monitor 115200.
4. Do not call `MDNS.begin()`.

Long-press BOOT (>5 s) to decommission the node (`Matter.decommission()`). This removes fabrics; it is not a full flash erase.

## Related

- [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) — BLE on, no key in the sketch (hub sends the dataset)
- [MatterOnNetworkWiFi](../MatterOnNetworkWiFi) — same on-network pattern for Wi-Fi
- [MatterOnNetworkEthernet](../MatterOnNetworkEthernet) — Ethernet (always on-network; no commissioning cluster)
