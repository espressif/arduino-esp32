# Matter Enhanced Smart Button Example

This example is still **one** physical button (BOOT), not several tagged switches. It enables Switch `FEATURE_ALL` and maps hold / repeated clicks to Matter events: `LongPress` / `LongRelease` and `MultiPressOngoing` / `MultiPressComplete` (count 2, 3, …).

For a short-click-only single button, see [MatterSmartButton](../MatterSmartButton). For **three** short-click buttons with Descriptor tags, see [MatterSmartButtonsTagList](../MatterSmartButtonsTagList).

Hubs advertise those features from the FeatureMap. Home Assistant typically exposes separate triggers. Apple Home, Alexa, and Google Home often treat a Generic Switch as a single press and may ignore long-press / multi-press.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       |
| -------- | ---------------------- | -------- | ---------------------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C5 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `WiFi.begin(ssid, password)`.
- **ESP32-C6:** prebuild is dual-stack. Without `selectNetwork()` this sketch uses **Wi-Fi + CHIPoBLE**. Thread stays unused.
- **ESP32-H2:** Thread + CHIPoBLE (no Wi-Fi).
- **ESP32-C5:** Wi-Fi + CHIPoBLE. Thread is not in that prebuild.

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (ESP32-C6 / ESP32-H2): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (ESP32-C6 / ESP32-H2): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Matter Generic Switch with **all momentary gesture features** enabled (`FEATURE_ALL`)
- Short click: `InitialPress` + `ShortRelease`
- Long press (default 1 s): `LongPress` + `LongRelease`
- Multi-press (default 300 ms window, up to 5 presses): `MultiPressOngoing` + `MultiPressComplete`
- Factory reset via 5 s hold (decommission, not a Matter event)
- Serial logging of each gesture for debugging

## Gesture Mapping

| Physical action | Matter event(s) |
| --- | --- |
| Press down (first in sequence) | `InitialPress` |
| Release (short) | `ShortRelease` |
| Hold ≥ 1 s | `LongPress` |
| Release after long hold | `LongRelease` |
| Press down (2nd+ within window) | `MultiPressOngoing` |
| Window expires after last release | `MultiPressComplete` |

## Configuration

Adjust timing constants at the top of the sketch:

```cpp
const uint32_t longPressMs = 1000;        // long-press threshold
const uint32_t multiPressWindowMs = 300;  // gap between clicks for multi-press
const uint8_t multiPressMax = 5;          // max presses reported to Matter
```

## Building and Flashing

Same steps as MatterSmartButton:

1. Open `MatterEnhancedSmartButton.ino` in the Arduino IDE
2. Select your board and **Huge APP (3 MB No OTA / 1 MB SPIFFS)** partition scheme
3. Enable **Erase All Flash Before Sketch Upload**
4. Upload

## Expected Serial Output

```
Initial press
Short release
Multi-press complete (count=1)

Initial press
Short release
Multi-press ongoing (count=2)
Short release
Multi-press complete (count=2)

Initial press
Long press
Long release
```

## Smart Home Integration

Hubs read the Switch cluster **FeatureMap** to determine supported gestures. With `FEATURE_ALL` (FeatureMap = 0x1E), Home Assistant and other controllers can expose separate automation triggers for short press, long press, and multi-press.

## Related Documentation

- [Matter Generic Switch Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_generic_switch.html)
- [MatterSmartButton](../MatterSmartButton) — simple short-click example
- [MatterSmartButtonsTagList](../MatterSmartButtonsTagList) — three tagged short-click buttons

## License

This example is licensed under the Apache License, Version 2.0.
