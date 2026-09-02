# Matter CHIPoBLE Release Example

Commission with CHIPoBLE, then let Matter shut NimBLE down and return BLE RAM to the heap. When reclaim finishes, `Matter.onBLEMemoryReleased()` runs so the sketch can allocate a larger buffer from `loop()`.

**Do not use the Arduino `BLE` library (`BLE.h` / `BLEDevice`) in this sketch.** Matter owns NimBLE. After BLE is deinitialized, `BLEDevice::init()` cannot start this boot.

## What it does

- Leaves CHIPoBLE enabled (default when `CONFIG_ENABLE_CHIPOBLE` is set)
- Calls `Matter.setBLEMemoryReleaseEnabled(true)` (the default) so BLE RAM is returned after a fabric exists
- Registers `Matter.onBLEMemoryReleased()` **before** `Matter.begin()`
- The callback only sets a flag (it runs on the CHIP task). `loop()` then `malloc`s a 16 KB demo buffer
- Prints `ESP.getFreeHeap()` / `getMinFreeHeap()` / `getMaxAllocHeap()`:
  - before `Matter.begin()`
  - after `Matter.begin()` (NimBLE is up; reclaim has **not** happened yet)
  - on `MATTER_COMMISSIONING_COMPLETE` (via `onEvent`)
  - after `onBLEMemoryReleased` (RAM reclaim finished; not instant)
- On Arduino IDE, original ESP32 / ESP32-S2 have no CHIPoBLE (`CONFIG_ENABLE_CHIPOBLE=n`): the sketch still runs over Wi-Fi. Original ESP32 can enable NimBLE + CHIPoBLE as an IDF component.

`onBLEMemoryReleased()` is the same moment as `MATTER_BLE_DEINITIALIZED`. Use the named callback for heap/alloc work; keep `onEvent()` for other Matter events. The callback may never run if CHIPoBLE is off or `setBLEMemoryReleaseEnabled(false)`.

Expect a delay of a few seconds between `MATTER_COMMISSIONING_COMPLETE` and the reclaim. Releasing the BLE regions while the NimBLE host task still runs would corrupt the heap, so the library waits for that task to exit (polled every 2 s, up to 30 s) before returning the RAM to the heap.

## Supported targets

| SoC                         | This sketch            | CHIPoBLE           | Also in prebuild              | Heap-after-BLE demo |
| --------------------------- | ---------------------- | ------------------ | ----------------------------- | ------------------- |
| ESP32 (Arduino IDE)         | Wi-Fi (SSID in sketch) | Off                | Ethernet (EMAC or SPI)        | No                  |
| ESP32 (IDF NimBLE+CHIPoBLE) | Wi-Fi (hub)            | On, then reclaimed | Ethernet (EMAC or SPI)        | Yes                 |
| ESP32-S2                    | Wi-Fi (SSID in sketch) | Off                | Ethernet (SPI)                | No                  |
| ESP32-S3                    | Wi-Fi (hub)            | On, then reclaimed | Ethernet (SPI)                | Yes                 |
| ESP32-C3                    | Wi-Fi (hub)            | On, then reclaimed | Ethernet (SPI)                | Yes                 |
| ESP32-C5                    | Wi-Fi (hub, default)   | On, then reclaimed | Thread (menu), Ethernet (SPI) | Yes                 |
| ESP32-C6                    | Wi-Fi (hub, default)   | On, then reclaimed | Thread, Ethernet (SPI)        | Yes                 |
| ESP32-H2                    | Thread (hub)           | On, then reclaimed | Ethernet (SPI)                | Yes                 |

This sketch leaves CHIPoBLE on when it is compiled in, then reclaims BLE RAM after a fabric exists. It does not call `selectNetwork()` or start Ethernet.

- Same BLE path without the heap demo: [MatterCHIPoBLEWiFi](../MatterCHIPoBLEWiFi).
- Disable CHIPoBLE and commission on Wi-Fi: [MatterOnNetworkWiFi](../MatterOnNetworkWiFi).
- ESP32-C6 also has Thread; this sketch keeps the default (Wi-Fi). For Thread see [MatterCHIPoBLEThread](../MatterCHIPoBLEThread).
- ESP32-C5 default **Matter Network** is Wi-Fi. Thread menu uses Thread + CHIPoBLE (same as this sketch with no `selectNetwork()`).
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../MatterOnNetworkEthernet).

Change the path with `Matter.selectNetwork()` before any accessory `begin()`. Do not also call `setBLECommissioningEnabled()`.

## Setup

1. Partition scheme: **Huge APP**. Enable **Erase All Flash Before Sketch Upload**.
2. Serial Monitor 115200.
3. Commission with the printed pairing code or QR (BLE when CHIPoBLE is in the build).
4. After commission (or on an already-commissioned reboot), wait for the “BLE memory released” lines, then the 16 KB `malloc`.

Long-press BOOT (>5 s) to decommission the node (`Matter.decommission()`). This removes fabrics; it is not a full flash erase. BLE comes back on the next boot until the node is commissioned again.

Call `Matter.setBLEMemoryReleaseEnabled(false)` before `Matter.begin()` to keep NimBLE after commissioning. That has no effect when `CONFIG_ENABLE_CHIPOBLE` is off (Arduino IDE original ESP32, ESP32-S2, or Bluetooth compiled out).

## Related

- [Matter overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [MatterOnNetworkWiFi](../MatterOnNetworkWiFi) — disable CHIPoBLE and commission on Wi-Fi (`onBLEMemoryReleased` still fires when RAM is reclaimed)
