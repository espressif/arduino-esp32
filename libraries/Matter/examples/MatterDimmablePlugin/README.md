# Matter Dimmable Plugin Example

This example demonstrates how to create a Matter-compatible dimmable plugin unit (power outlet with level control) device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and state persistence for dimmable power control applications.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | Relay/Dimmer | Status |
| --- | ---- | ------ | ----------------- | ------------ | ------ |
| ESP32 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C5 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C6 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Required | Supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Matter protocol implementation for a dimmable plugin unit (power outlet with level control) device
- Support for both Wi-Fi and Thread(*) connectivity
- On/off control and power level control (0-255 levels)
- State persistence using `Preferences` library
- Button control for toggling plugin and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Power relay/dimmer module or RGB LED for visualization (for testing, uses built-in RGB LED if available)
- User button for manual control (uses BOOT button by default)

## Pin Configuration

- **RGB LED/Relay/Dimmer Pin**: Uses `RGB_BUILTIN` if defined (for testing with RGB LED visualization), otherwise pin 2. For production use, connect this to a PWM-capable pin for dimmer control or relay control pin.
- **Button**: Uses `BOOT_PIN` by default

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `Preferences`
   - `Wi-Fi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi credentials** (if not using BLE commissioning - mandatory for ESP32 | ESP32-S2):
   ```cpp
   const char *ssid = "your-ssid";         // Change to your Wi-Fi SSID
   const char *password = "your-password"; // Change to your Wi-Fi password
   ```

2. **Power relay/dimmer pin configuration** (if not using built-in LED):
   For production use, change this to a PWM-capable GPIO pin connected to your dimmer control module:
   ```cpp
   const uint8_t pluginPin = 2;  // Set your PWM-capable pin here for dimmer control
   ```

   **Note**: The example uses `RGB_BUILTIN` if available on your board (e.g., ESP32-S3, ESP32-C3) to visually demonstrate the level control. The RGB LED brightness will change based on the power level (0-255). For boards without RGB LED, it falls back to a regular pin with PWM support.

3. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for the Plugin On/Off manual control. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterDimmablePlugin.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

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
Initial state: OFF | level: 64
Matter Node is commissioned and connected to the network. Ready for use.
Plugin OnOff changed to ON
Plugin Level changed to 128
User Callback :: New Plugin State = ON, Level = 128
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Toggle plugin on/off
- **Long press (>5 seconds)**: Factory reset the device (decommission)

### State Persistence

The device saves the last known on/off state and power level using the `Preferences` library. After a power cycle or restart:

- The device will restore to the last saved state (ON or OFF) and power level
- Default state is OFF with level 64 (25%) if no previous state was saved
- The Matter controller will be notified of the restored state
- The relay/dimmer will reflect the restored state and level

### Power Relay/Dimmer Integration

For production use with a power relay or dimmer module:

1. **For Dimmer Control (PWM-based)**:
   - Connect the dimmer module to your ESP32:
     - Dimmer VCC → ESP32 3.3 V or 5 V (check dimmer module specifications)
     - Dimmer GND → ESP32 GND
     - Dimmer PWM/Control → ESP32 GPIO pin with PWM support (configured as `pluginPin`)
   - Update the pin configuration in the sketch:
     ```cpp
     const uint8_t pluginPin = 2;  // Your PWM-capable pin for dimmer control
     ```
   - The level (0-255) will control the dimmer output power (0% to 100%)

2. **For Relay Control (On/Off only)**:
   - Connect the relay module to your ESP32:
     - Relay VCC → ESP32 3.3 V or 5 V (check relay module specifications)
     - Relay GND → ESP32 GND
     - Relay IN → ESP32 GPIO pin (configured as `pluginPin`)
   - Update the pin configuration in the sketch:
     ```cpp
     const uint8_t pluginPin = 2;  // Your relay control pin
     ```
   - Note: When using a relay, the level control will still work but the relay will only switch on/off based on the state

3. **Test the relay/dimmer** by controlling it via Matter app - the device should respond to both on/off and level changes

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a dimmable outlet/switch in your Home app
7. You can control both the on/off state and power level (0-100%)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The dimmable plugin will appear in your Alexa app
6. You can control power level using voice commands like "Alexa, set outlet to 50 percent"

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control power level using voice commands or the slider in the app

## Code Structure

The MatterDimmablePlugin example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, relay/dimmer pin), configures Wi-Fi (if needed), initializes `Preferences` library, sets up the Matter plugin endpoint with the last saved state (defaults to OFF with level 64 if not previously saved), registers callback functions, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, handles button input for toggling the plugin and factory reset, and allows the Matter stack to process events.

3. **Callbacks**:
   - `setPluginState()`: Controls the physical relay/dimmer based on the on/off state and power level, saves the state to `Preferences` for persistence, and prints the state change to Serial Monitor.
   - `onChangeOnOff()`: Handles on/off state changes.
   - `onChangeLevel()`: Handles power level changes (0-255).

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Relay/Dimmer not responding**: Verify pin configurations and connections. For dimmer modules, ensure the pin supports PWM (analogWrite). For relay modules, ensure proper power supply and wiring
- **Level control not working**: For dimmer control, verify the pin supports PWM. Check that `analogWrite()` or `rgbLedWrite()` (for RGB LED) is working correctly on your board. On boards with RGB LED, the brightness will change based on the level value (0-255)
- **State not persisting**: Check that the `Preferences` library is properly initialized and that flash memory is not corrupted
- **Relay not switching**: For relay modules, verify the control signal voltage levels match your relay module requirements (some relays need 5 V, others work with 3.3 V)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Dimmable Plugin Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_dimmable_plugin.html)

## License

This example is licensed under the Apache License, Version 2.0.
