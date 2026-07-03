# Matter Enhanced Smart Button Example

This example demonstrates a Matter-compatible smart button (generic switch) with **full gesture support**: short click, long press, and multi-press (double/triple click). It maps physical button actions to all Matter Switch cluster momentary events.

For a minimal short-click-only implementation, see the [MatterSmartButton](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSmartButton) example.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | Status |
| --- | ---- | ------ | ----------------- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C6 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C5 | ❌ | ✅ | ✅ | Supported (Thread only) |
| ESP32-H2 | ❌ | ✅ | ✅ | Supported (Thread only) |

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
- [Matter Smart Button (simple example)](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSmartButton)

## License

This example is licensed under the Apache License, Version 2.0.
