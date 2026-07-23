# Matter Smart Buttons with TagList Example

This example demonstrates how to create two Matter-compatible smart buttons (Generic Switch), On and Off, on the same ESP32 SoC microcontroller, and how to use the Descriptor cluster `TagList` attribute to disambiguate them.\
Both buttons expose the same Matter device type (Generic Switch), so without tagging, a controller has no standard way to tell which one is "On" and which one is "Off". Tagging each endpoint with a semantic tag (On/Off) solves this.

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

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project using Arduino as an IDF Component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Wi-Fi 2.4 GHz and 5 GHz support, the ESP32 Arduino Matter Library has been pre compiled using Thread only. In order to configure it for Wi-Fi operation it is necessary to build the project using Arduino as an ESP-IDF component and disable Thread network, keeping only Wi-Fi station.

## Features

- Two independent Matter Generic Switch endpoints (On and Off) on a single Matter node
- Disambiguates sibling endpoints using the Descriptor cluster `TagList` attribute (`MatterGenericSwitch::setTagList()`)
- Support for both Wi-Fi and Thread(*) connectivity
- **Simple short-click** gesture per button: `InitialPress` on press, `ShortRelease` on release
- Dedicated button for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home

For gesture support (long-press, multi-press), see the [MatterEnhancedSmartButton](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedSmartButton) example.\
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Two push buttons for On/Off events (default pins: GPIO4 and GPIO5)
- A third button (uses BOOT button by default) to trigger factory reset / decommissioning

## Pin Configuration

- **On button**: GPIO4 by default (`buttonOnPin`)
- **Off button**: GPIO5 by default (`buttonOffPin`)
- **Decommission button**: `BOOT_PIN` by default (`decommissionButtonPin`)

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `Wi-Fi` (only for ESP32 and ESP32-S2)

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

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. The Wi-Fi connection messages will be displayed only for ESP32 and ESP32-S2. Other targets will use Matter CHIPoBLE to automatically setup the IP Network. You should see output similar to the following, which provides the necessary information for commissioning:

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
```

## Using the Device

### Manual Control

Each button is a **simple implementation** — short click only:

- **Press down**: sends `InitialPress` to the Matter controller
- **Release**: sends `ShortRelease` to the Matter controller
- **Decommission button held (>5 seconds)**: Factory reset the device (decommission) — this is not a Matter gesture

### Tagging the Buttons (TagList)

Both `ButtonOn` and `ButtonOff` are `MatterGenericSwitch` endpoints, so they expose the same Matter device type. To let a controller tell them apart, each endpoint is tagged right after `begin()`, using a `MatterTag` entry (namespace ID + tag ID, from the Matter [Standard Namespaces specification](https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces)):

```cpp
MatterTag onTagList[] = {
  {kNamespaceSwitches, kTagSwitchOn},  // Switches Namespace (0x43): On
};
ButtonOn.begin();
ButtonOn.setTagList(onTagList, 1);
```

`ButtonOff` is tagged the same way, using the Off tag instead. `setTagList()` must be called after `begin()`, since the endpoint (and its Descriptor cluster) must already exist.

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. After commissioning, both buttons appear as separate switch endpoints that you can use to trigger automations — the tags help the controller's UI label them correctly (e.g. "On"/"Off") instead of showing two indistinguishable switches.

## Code Structure

The MatterSmartButtonsTagList example consists of the following main components:

1. **`setup()`**: Initializes hardware (both buttons and the decommission button), configures Wi-Fi (if needed), initializes both Matter Generic Switch endpoints, tags them with `setTagList()`, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, handles both buttons' input via `handleButton()`, and checks the dedicated decommission button.

3. **Button Event Handling** (`handleButton()`):
   - Detects button press and release with debouncing (250 ms)
   - Sends `InitialPress` on press down and `ShortRelease` on release
   - Shared between both buttons, parameterized by pin, endpoint, and name

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Button clicks not registering**: Check Serial Monitor for "button pressed" and "button released" messages. Verify button wiring and debounce time
- **Both buttons show as identical/unlabeled in the controller app**: Not all Matter controllers render semantic tags in their UI; the tags are still present in the TagList attribute and can be read by controllers that support it
- **Failed to commission**: Try factory resetting the device by holding the decommission button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Generic Switch Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_generic_switch.html)
- [Matter Smart Button](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSmartButton) — single button example
- [Matter Enhanced Smart Button](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedSmartButton) — long press and multi-press example
- [Matter Standard Namespaces specification](https://github.com/CHIP-Specifications/connectedhomeip-spec/blob/master/src/namespaces)

## License

This example is licensed under the Apache License, Version 2.0.
