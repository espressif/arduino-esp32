# Matter Smart Button Example

This example demonstrates how to create a Matter-compatible smart button (generic switch) device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, sending button click events to smart home ecosystems, and triggering automations based on button presses.

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

- Matter protocol implementation for a smart button (generic switch) device
- Support for both Wi-Fi and Thread(*) connectivity
- **Simple short-click** gesture: `InitialPress` on press, `ShortRelease` on release
- Button control for triggering events and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
- Automation trigger support - button presses can trigger actions in smart home apps

For long-press and multi-press gestures, see the [MatterEnhancedSmartButton](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedSmartButton) example.
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for triggering events (uses BOOT button by default)

## Pin Configuration

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

2. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for triggering click events and factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterSmartButton.ino` sketch in the Arduino IDE.
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
Button pressed — sending InitialPress
Button released — sending ShortRelease
Button pressed — sending InitialPress
Button released — sending ShortRelease
```

## Using the Device

### Manual Control

This is a **simple implementation** — short click only. The button sends Matter events on press and release:

- **Press down**: sends `InitialPress` to the Matter controller
- **Release**: sends `ShortRelease` to the Matter controller
- **Long press (>5 seconds)**: Factory reset the device (decommission) — this is not a Matter gesture

For long-press and multi-press Matter gestures, use the [MatterEnhancedSmartButton](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedSmartButton) example.

### Button Click Events

When you press and release the button:

1. On press (after debounce): `SmartButton.press()` sends `InitialPress`
2. On release (after debounce): `SmartButton.release()` sends `ShortRelease`
3. The Matter controller receives both events and can trigger automations
4. Each event is logged to Serial Monitor for debugging

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. After commissioning, you can set up automations that trigger when the button is pressed.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a switch/button in your Home app
7. Set up automations: Go to Automation tab > Create Automation > When an accessory is controlled > Select the smart button > Choose the action (e.g., turn on lights, activate scene)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The smart button will appear in your Alexa app
6. Create routines: Tap More > Routines > Create Routine > When this happens > Select the smart button > Add action (e.g., turn on lights, control other devices)

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. Create automations: Tap Automations > Create Automation > When button is pressed > Choose action

## Code Structure

The MatterSmartButton example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), initializes the Matter Generic Switch endpoint, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, handles button input for sending click events and factory reset, and allows the Matter stack to process events.

3. **Button Event Handling**:
   - Detects button press and release with debouncing (50 ms)
   - Sends `InitialPress` on press down and `ShortRelease` on release
   - Handles long press (>5 seconds) for factory reset (decommission)

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Button clicks not registering**: Check Serial Monitor for "Button pressed" and "Button released" messages. Verify button wiring and debounce time
- **Automations not triggering**: Ensure the device is commissioned and that automations are properly configured in your Matter app. The button sends events, but automations must be set up in the app
- **Button not responding**: Verify button pin configuration and connections. Check that the button is properly connected with pull-up resistor (INPUT_PULLUP mode)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection
- **Multiple clicks registered for single press**: Increase the debounce time in the code if needed

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Generic Switch Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_generic_switch.html)
- [Matter Enhanced Smart Button](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedSmartButton) — long press and multi-press example

## License

This example is licensed under the Apache License, Version 2.0.
