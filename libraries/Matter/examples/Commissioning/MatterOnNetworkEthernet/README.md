# Matter On-Network Ethernet Example

Commission a Matter On/Off Light over Ethernet. Pair this with [MatterOnNetworkWiFi](../MatterOnNetworkWiFi): the accessory code is the same; only the network bring-up differs.

**Do not start Arduino `ESPmDNS`.** CHIP owns the mDNS responder. `MDNS.begin()` overwrites the hostname; `MDNS.end()` destroys CHIP's services. `Matter.begin()` starts CHIP's responder; the sketch must not call `MDNS.begin()`.

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`) in this sketch.**

## What it does

- Blinks `LED_BUILTIN` (or GPIO 2) once so the board is visibly alive
- Checks `Matter.isNetworkSupported(MATTER_NETWORK_ETHERNET)` (`CONFIG_ETH_ENABLED`)
- Calls `Matter.selectNetwork(MATTER_NETWORK_ETHERNET)` before any accessory `begin()` (this turns CHIPoBLE off)
- Starts Ethernet, then IPv6:
  - **Internal EMAC** (original ESP32): when `CONFIG_ETH_USE_ESP32_EMAC` is set and the variant defines `ETH_PHY_MDC` / `ETH_PHY_MDIO`, calls `ETH.begin()` with no arguments
  - **SPI PHY** (default in this sketch): `SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI)` then `ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI)` — default chip is W5500
- `ETH.enableIPv6()` and `Matter.waitForNetwork(30000)`
- Starts the On/Off Light and `Matter.begin()`
- Prints on-network pairing codes

There is no Ethernet Network Commissioning cluster. The commissioner must already reach the device on IP.

## PHY macros

Define overrides **before** `#include <ETH.h>`, or use a variant that already defines them.

Default pins (used when `ETH_PHY_TYPE` is not predefined) are a W5500 on SPI:

| Macro | Default |
| --- | --- |
| `ETH_PHY_TYPE` | `ETH_PHY_W5500` |
| `ETH_PHY_ADDR` | `1` |
| `ETH_PHY_CS` | `15` |
| `ETH_PHY_IRQ` | `4` |
| `ETH_PHY_RST` | `5` |
| `ETH_SPI_SCK` | `14` |
| `ETH_SPI_MISO` | `12` |
| `ETH_SPI_MOSI` | `13` |

Internal EMAC on original ESP32 uses `ETH_PHY_MDC` / `ETH_PHY_MDIO` / `ETH_CLK_MODE` instead of SPI. The sketch then takes the `ETH.begin()` (no-arg) path.

## Supported targets

`CONFIG_ETH_ENABLED` is on for all Arduino Matter targets. Ethernet commissioning is on-network only (no Network Commissioning cluster).

| SoC      | This sketch | CHIPoBLE | Also in prebuild | PHY                         |
| -------- | ----------- | -------- | ---------------- | --------------------------- |
| ESP32    | Ethernet    | Off      | Wi-Fi            | EMAC or SPI (tested: W5500) |
| ESP32-S2 | Ethernet    | Off      | Wi-Fi            | SPI                         |
| ESP32-S3 | Ethernet    | Off      | Wi-Fi            | SPI                         |
| ESP32-C3 | Ethernet    | Off      | Wi-Fi            | SPI                         |
| ESP32-C5 | Ethernet    | Off      | Wi-Fi            | SPI                         |
| ESP32-C6 | Ethernet    | Off      | Wi-Fi, Thread    | SPI                         |
| ESP32-H2 | Ethernet    | Off      | Thread           | SPI                         |

This sketch calls `Matter.selectNetwork(MATTER_NETWORK_ETHERNET)` (CHIPoBLE **off**), then `ETH.begin()`, `enableIPv6()`, and `waitForNetwork()` before `Matter.begin()`. You still need a PHY.

- Same on-network pattern on Wi-Fi: [MatterOnNetworkWiFi](../MatterOnNetworkWiFi).
- Same on-network pattern on Thread: [MatterOnNetworkThread](../MatterOnNetworkThread).
- BLE commissioning (no credentials in the sketch): [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi) / [MatterCHIPoBLEThread](../MatterCHIPoBLEThread).

Change the path with `Matter.selectNetwork()` before any accessory `begin()`. Do not also call `setBLECommissioningEnabled()`.

## Setup

1. Set `ETH_PHY_*` and, for SPI, `ETH_SPI_*` for your module. For an EMAC board, use a variant that defines `ETH_PHY_MDC` / `ETH_PHY_MDIO`.
2. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
3. Serial Monitor 115200.
4. Do not call `MDNS.begin()`.

Bring Ethernet up and wait for IPv6 **before** `Matter.begin()`. After a cable unplug/replug, or a reboot of an already-commissioned node, the library re-publishes DNS-SD on `IP_EVENT_GOT_IP6`.

Long-press BOOT (>5 s) to factory-reset.

## Related

- [MatterOnNetworkWiFi](../MatterOnNetworkWiFi) — same on-network pattern for Wi-Fi
- [MatterOnNetworkThread](../MatterOnNetworkThread) — same on-network pattern for Thread
- [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi) / [MatterCHIPoBLEThread](../MatterCHIPoBLEThread) — BLE commissioning (no credentials in the sketch)
