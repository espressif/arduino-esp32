# Matter Smart Buttons with TagList Example

This example demonstrates how to create three Matter-compatible smart buttons (Generic Switch) — On, Off, and a custom-labeled Scene — on the same ESP32 SoC microcontroller, and how to use the Descriptor cluster `TagList` attribute to disambiguate them.\
All three buttons expose the same Matter device type (Generic Switch), so without tagging, a controller has no standard way to tell them apart. Tagging each endpoint with a semantic tag (On/Off, or a Custom tag with a label) solves this.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild              |
| -------- | ---------------------- | -------- | ----------------------------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI)        |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)                |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)                |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)                |
| ESP32-C5 | Wi-Fi (hub, default)   | On       | Thread (menu), Ethernet (SPI) |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI)        |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)                |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `WiFi.begin(ssid, password)`.
- **ESP32-C6:** prebuild is dual-stack. Without `selectNetwork()` this sketch uses **Wi-Fi + CHIPoBLE**. Thread stays unused.
- **ESP32-H2:** Thread + CHIPoBLE (no Wi-Fi).
- **ESP32-C5:** Wi-Fi + CHIPoBLE when Tools → Matter Network is Wi-Fi (default). Thread + CHIPoBLE when Matter Network is Thread (Matter.isThreadEnabled() / CONFIG_ENABLE_MATTER_OVER_THREAD).

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (ESP32-C6 / ESP32-H2 / ESP32-C5 Thread menu): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (ESP32-C6 / ESP32-H2 / ESP32-C5 Thread menu): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Three independent Matter Generic Switch endpoints (On, Off, and Scene) on a single Matter node
- Disambiguates sibling endpoints using the Descriptor cluster `TagList` attribute (`MatterEndPoint::setTagList()`)
- Shows both standard Switches tags (`On`/`Off`) and a labeled Custom tag (`MatterTags::Switches::createCustomTag()`)
- Default network and CHIPoBLE as in the Supported Targets table (ESP32-C6 dual-stack uses Wi-Fi unless you call `selectNetwork()`; ESP32-C5 default menu is Wi-Fi, Thread menu is Thread)
- **Simple short-click** gesture per button: `InitialPress` on press, `ShortRelease` on release
- Dedicated button for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home

For gesture support (long-press, multi-press), see the [MatterEnhancedSmartButton](../MatterEnhancedSmartButton) example.\

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Three push buttons for On/Off/Scene events (default pins: GPIO4, GPIO5, and GPIO2)
- A fourth button (uses BOOT button by default) to trigger factory reset / decommissioning

## Pin Configuration

- **On button**: GPIO4 by default (`buttonOnPin`)
- **Off button**: GPIO5 by default (`buttonOffPin`)
- **Scene button**: GPIO2 by default (`buttonScenePin`) — on some ESP32 boards this is the onboard LED / a strapping pin; change it if needed
- **Decommission button**: `BOOT_PIN` by default (`decommissionButtonPin`)

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `WiFi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi credentials** (if not using BLE commissioning - mandatory for ESP32 | ESP32-S2):
   ```cpp
   const char *ssid = "your-ssid";         // Change to your Wi-Fi SSID
   const char *password = "your-password"; // Change to your Wi-Fi password
   ```

2. **Button pin configuration** (optional):
   ```cpp
   const uint8_t buttonOnPin = 4;                   // Set your On button pin here
   const uint8_t buttonOffPin = 5;                  // Set your Off button pin here
   const uint8_t buttonScenePin = 2;                // Set your Scene button pin here
   const uint8_t decommissionButtonPin = BOOT_PIN;  // Set your decommission button pin here
   ```

## Building and Flashing

1. Open the `MatterSmartButtonsTagList.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. Wi-Fi connection messages appear only on ESP32 and ESP32-S2. CHIPoBLE targets get the operational network from the hub (Wi-Fi, or Thread on ESP32-H2). You should see output similar to the following, which provides the necessary information for commissioning:

```
Connecting to your-wifi-ssid
.......
Wi-Fi connected
IP address: 192.168.1.100

Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A6FCJ142C00KA0648G00
Matter Node not commissioned yet. Waiting for commissioning.
Matter Node not commissioned yet. Waiting for commissioning.
...
Matter Node is commissioned and connected to the network. Ready for use.
On button pressed. Sending InitialPress to the Matter Controller!
On button released. Sending ShortRelease to the Matter Controller!
Off button pressed. Sending InitialPress to the Matter Controller!
Off button released. Sending ShortRelease to the Matter Controller!
Scene 1 button pressed. Sending InitialPress to the Matter Controller!
Scene 1 button released. Sending ShortRelease to the Matter Controller!
```

## Using the Device

### Manual Control

Each button is a **simple implementation** — short click only:

- **Press down**: sends `InitialPress` to the Matter controller
- **Release**: sends `ShortRelease` to the Matter controller
- **Decommission button held (>5 seconds)**: Factory reset the device (decommission) — this is not a Matter gesture

### Tagging the Buttons (TagList)

`ButtonOn`, `ButtonOff`, and `ButtonScene` are `MatterGenericSwitch` endpoints, so they expose the same Matter device type. To let a controller tell them apart, each endpoint is tagged right after `begin()`, using a `MatterTag` entry. `setTagList()` is defined on the shared `MatterEndPoint` base class, so it is available on any endpoint type, not just `MatterGenericSwitch`.

The `MatterTags` namespace (see `MatterTags.h`) provides named constants for the common Matter semantic tag namespaces — no need to hardcode namespace/tag numbers from the Matter [Standard Namespaces specification](https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces):

```cpp
ButtonOn.begin();
ButtonOn.setTagList({MatterTags::Switches::On});

ButtonOff.begin();
ButtonOff.setTagList({MatterTags::Switches::Off});

// Switches Custom tag with a user-visible label (string literal must outlive the endpoint)
ButtonScene.begin();
ButtonScene.setTagList({MatterTags::Switches::createCustomTag("Scene 1")});
```

`setTagList()` must be called after `begin()` and before `Matter.begin()`. This example uses Generic Switch, which already enables TagList during `begin()`; on other endpoint types the first `setTagList()` call enables it. At most 3 tags are accepted per endpoint (`MatterEndPoint::MAX_TAG_LIST_SIZE`, matching esp-matter). For a tag outside the predefined namespaces, or a custom label in another namespace, use `MatterTags::createTag(namespaceId, tag, label)`. A `setTagList(const MatterTag *tagList, uint8_t count)` overload is also available for building the list at runtime (e.g. a size known only at runtime, or a list shared and reused across endpoints).

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. After commissioning, the three buttons appear as separate switch endpoints that you can use to trigger automations — the tags help the controller's UI label them correctly (e.g. "On"/"Off"/"Scene 1") instead of showing indistinguishable switches.

## Code Structure

The MatterSmartButtonsTagList example consists of the following main components:

1. **`setup()`**: Initializes hardware (On, Off, Scene, and the decommission button), configures Wi-Fi (if needed), initializes the three Matter Generic Switch endpoints, tags them with `setTagList()`, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, handles all three buttons' input via `handleButton()`, and checks the dedicated decommission button.

3. **Button Event Handling** (`handleButton()`):
   - Detects button press and release with debouncing (250 ms)
   - Sends `InitialPress` on press down and `ShortRelease` on release
   - Shared between the buttons, parameterized by pin, endpoint, and name

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Button clicks not registering**: Check Serial Monitor for "button pressed" and "button released" messages. Verify button wiring and debounce time
- **Buttons show as identical/unlabeled in the controller app**: Not all Matter controllers render semantic tags in their UI; the tags are still present in the TagList attribute and can be read by controllers that support it
- **Failed to commission**: Try factory resetting the device by holding the decommission button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Generic Switch Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_generic_switch.html)
- [MatterSmartButton](../MatterSmartButton) — single button example
- [MatterEnhancedSmartButton](../MatterEnhancedSmartButton) — long press and multi-press example
- [Matter Standard Namespaces specification](https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces)

## License

This example is licensed under the Apache License, Version 2.0.
