# Matter Early BLE Release

One **On/Off Light**. Two ways to handle BLE RAM. Serial `[heap]` also prints PSRAM when that heap exists.

| `MATTER_EARLY_BLE_RELEASE` | Commissioning | BLE RAM |
| -------------------------- | ------------- | ------- |
| **1** (default) | On-network Wi-Fi (`selectNetwork(MATTER_NETWORK_WIFI, true)` + `WiFi.begin()`). No CHIPoBLE. | Released at `initArduino()` (`bleInUse()` returns `false`). |
| **0** | CHIPoBLE. No sketch Wi-Fi. Hub sends credentials over BLE. | Library reclaim after `Matter.begin()` / commission. |

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`).** Neither mode hands the radio to Arduino BLE.

## `MATTER_EARLY_BLE_RELEASE`

Set these **just after the includes**:

1. **`MATTER_EARLY_BLE_RELEASE`** (default **1**, or `-DMATTER_EARLY_BLE_RELEASE=0` / `=1`)
2. **`ssid` / `password`** — mode **1** only
3. **`ledPin`** — only if the board has no `LED_BUILTIN`

Mode 1 must use `extern "C" bool bleInUse(void)`. A C++ `bool bleInUse()` is mangled and does not override the HAL.

Do not also call `setBLECommissioningEnabled()`.

## PSRAM and `[heap]`

`[heap]` prints `int=` / `intMax=` (internal RAM) and `psram=` when `psramFound()`.

- **Tools → PSRAM** (`BOARD_HAS_PSRAM`) is printed as Enabled/Disabled. On ESP32-S3 the chip can still initialize when the menu is Disabled.
- If there is no SPIRAM heap, the line shows `psram=none`.

## Supported targets

| SoC | Mode 1 | Mode 0 |
| --- | ------ | ------ |
| ESP32 / ESP32-S2 | Sketch Wi-Fi (no CHIPoBLE in the prebuild) | Same |
| ESP32-S3 / C3 / C5 / C6 | On-network Wi-Fi, BLE free at boot | CHIPoBLE |
| ESP32-H2 | Halt (no Wi-Fi) — use mode **0** | CHIPoBLE / Thread |

Arduino IDE: C5 default is **Matter Network → Wi-Fi**. C6 has no Matter Network menu.

## Setup

1. After the includes: set `MATTER_EARLY_BLE_RELEASE`. For mode **1**, set `ssid` / `password`.
2. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
3. Serial Monitor 115200. The light drives `ledPin`. After `Matter.begin()`, the sketch waits for commissioning / CASE via `matterWaitUntilReady()`. Long-press BOOT (>5 s) to decommission. `loop()` calls `matterRestartIfNoFabric()` if the hub removed the fabric.

## Related

- [Matter overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi) — CHIPoBLE off, no early `bleInUse()`
- [MatterCHIPoBLERelease](../../Commissioning/MatterCHIPoBLERelease) — CHIPoBLE on, reclaim after commission
