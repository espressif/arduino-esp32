# Matter Minimum Example

This example demonstrates the smallest code required to create a Matter-compatible device using an ESP32 SoC microcontroller.\
The application showcases the minimal implementation for Matter commissioning and device control via smart home ecosystems. This is an ideal starting point for understanding Matter basics and building more complex devices.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | LED | Status |
| --- | ---- | ------ | ----------------- | --- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Optional | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Optional | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Optional | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Optional | Fully supported |
| ESP32-C5 | ✅ | ❌ | ✅ | Optional | Fully supported |
| ESP32-C6 | ✅ | ❌ | ✅ | Optional | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Optional | Supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Minimal Matter protocol implementation for an on/off light device
- Support for both Wi-Fi and Thread(*) connectivity
- Simple on/off control via Matter app
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
- Minimal code footprint - ideal for learning Matter basics
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Optional: LED connected to GPIO pin (or using built-in LED) for visual feedback
- Optional: User button for factory reset (uses BOOT button by default)

## Pin Configuration

- **LED**: Uses `LED_BUILTIN` if defined, otherwise pin 2
- **Button**: Uses `BOOT_PIN` by default (only for factory reset)

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

2. **LED pin configuration** (if not using built-in LED):
   ```cpp
   const uint8_t ledPin = 2;  // Set your LED pin here
   ```

3. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterMinimum.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. The Wi-Fi connection messages will be displayed only for ESP32 and ESP32-S2. Other targets will use Matter CHIPoBLE to automatically setup the IP Network. You should see output similar to the following, which provides the necessary information for commissioning:

```
Connecting to your-wifi-ssid
.......
Wi-Fi connected

Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A6FCJ142C00KA0648G00
```

After commissioning, the device will be ready for control via Matter apps. No additional status messages are printed in this minimal example.

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

Note: This minimal example does not include button toggle functionality. To add manual toggle control, you can extend the code with additional button handling logic.

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an on/off light in your Home app

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The light will appear in your Alexa app

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup

## Code Structure

The MatterMinimum example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, LED), configures Wi-Fi (if needed), initializes the Matter on/off light endpoint, registers the callback function, and starts the Matter stack. Displays commissioning information if not yet commissioned.

2. **`loop()`**: Handles button input for factory reset (long press >5 seconds) and allows the Matter stack to process events. This minimal example does not include commissioning state checking in the loop - it only checks once in setup.

3. **`onOffLightCallback()`**: Simple callback function that controls the LED based on the on/off state received from the Matter controller. This is the minimal callback implementation.

## Extending the Example

This minimal example can be extended with additional features:

- **State persistence**: Add `Preferences` library to save the last known state
- **Button toggle**: Add button press detection to toggle the light manually
- **Commissioning status check**: Add periodic checking of commissioning state in the loop
- **Multiple endpoints**: Add more Matter endpoints to the same node
- **Enhanced callbacks**: Add more detailed callback functions for better control

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **LED not responding**: Verify pin configurations and connections. The LED will only respond to Matter app commands after commissioning
- **Failed to commission**: Try erasing the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection
- **LED not turning on/off**: Ensure the device is commissioned and you're controlling it via a Matter app. The minimal example only responds to Matter controller commands, not local button presses

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)

## License

This example is licensed under the Apache License, Version 2.0.
