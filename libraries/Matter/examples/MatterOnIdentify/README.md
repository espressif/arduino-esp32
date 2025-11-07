# Matter On Identify Example

This example demonstrates how to implement the Matter Identify cluster callback for an on/off light device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and the Identify feature that makes the LED blink when the device is identified from a Matter app.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | LED | Status |
| --- | ---- | ------ | ----------------- | --- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C5 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C6 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Required | Supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Matter protocol implementation for an on/off light device
- Support for both Wi-Fi and Thread(*) connectivity
- On Identify callback implementation - LED blinks when device is identified
- Visual identification feedback (red blinking for RGB LED, toggling for regular LED)
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- LED connected to GPIO pin (or using built-in LED) for visual feedback
- Optional: RGB LED for red blinking identification (uses RGB_BUILTIN if available)
- User button for factory reset (uses BOOT button by default)

## Pin Configuration

- **LED**: Uses `LED_BUILTIN` if defined, otherwise pin 2
- **Button**: Uses `BOOT_PIN` by default

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

1. Open the `MatterOnIdentify.ino` sketch in the Arduino IDE.
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
Matter Node not commissioned yet. Waiting for commissioning.
Matter Node not commissioned yet. Waiting for commissioning.
...
Matter Node is commissioned and connected to the network. Ready for use.
```

When you trigger the Identify command from a Matter app, you should see:
```
Identify Cluster is Active
Identify Cluster is Inactive
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Identify Feature

The Identify feature allows you to visually identify a specific device from your Matter app. When you trigger the Identify command:

1. **For RGB LED (RGB_BUILTIN)**: The LED will blink in red color
2. **For regular LED**: The LED will toggle on/off

The blinking continues while the Identify cluster is active (typically 3-15 seconds depending on the app). When the Identify period ends, the LED automatically returns to its previous state (on or off).

### How to Trigger Identify

#### Apple Home

1. Open the Home app on your iOS device
2. Find your device in the device list
3. Long press on the device
4. Tap "Identify" or look for the identify option in device settings
5. The LED will start blinking

#### Amazon Alexa

1. Open the Alexa app
2. Navigate to your device
3. Look for "Identify" or "Find Device" option in device settings
4. The LED will start blinking

#### Google Home

1. Open the Google Home app
2. Select your device
3. Look for "Identify" or "Find Device" option
4. The LED will start blinking

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an on/off light in your Home app
7. Use the Identify feature to visually locate the device

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The light will appear in your Alexa app
6. Use the Identify feature to visually locate the device

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. Use the Identify feature to visually locate the device

## Code Structure

The MatterOnIdentify example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, LED), configures Wi-Fi (if needed), initializes the Matter on/off light endpoint, registers the on/off callback and the Identify callback, and starts the Matter stack.

2. **`loop()`**: Handles the Identify blinking logic (if identify flag is active, blinks the LED every 500 ms), handles button input for factory reset, and allows the Matter stack to process events.

3. **Callbacks**:
   - `onOffLightCallback()`: Controls the physical LED based on on/off state from Matter controller.
   - `onIdentifyLightCallback()`: Handles the Identify cluster activation/deactivation. When active, sets the identify flag to start blinking. When inactive, stops blinking and restores the original light state.

4. **Identify Blinking Logic**:
   - For RGB LEDs: Blinks in red color (brightness 32) when identify is active
   - For regular LEDs: Toggles on/off when identify is active
   - Blinking rate: Every 500 ms (determined by the delay in loop)

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **LED not responding**: Verify pin configurations and connections
- **Identify feature not working**: Ensure the device is commissioned and you're using a Matter app that supports the Identify cluster. Some apps may not have a visible Identify button
- **LED not blinking during identify**: Check Serial Monitor for "Identify Cluster is Active" message. If you don't see it, the Identify command may not be reaching the device
- **LED state not restored after identify**: The code uses a double-toggle to restore state. If this doesn't work, ensure the light state is properly tracked
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter On/Off Light Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_on_off_light.html)

## License

This example is licensed under the Apache License, Version 2.0.
