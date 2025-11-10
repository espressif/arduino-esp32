# Matter On/Off Plugin Example

This example demonstrates how to create a Matter-compatible on/off plugin unit (power relay) device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and state persistence for power control applications.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | Relay/LED | Status |
| --- | ---- | ------ | ----------------- | --------- | ------ |
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

- Matter protocol implementation for an on/off plugin unit (power relay) device
- Support for both Wi-Fi and Thread(*) connectivity
- Simple on/off control for power management
- State persistence using `Preferences` library
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Power relay module or LED for visualization (for testing, uses built-in LED)
- User button for factory reset (uses BOOT button by default)

## Pin Configuration

- **Power Relay/Plugin Pin**: Uses `LED_BUILTIN` if defined (for testing), otherwise pin 2. For production use, connect this to your relay control pin.
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

2. **Power relay pin configuration** (if not using built-in LED):
   For production use, change this to the GPIO pin connected to your relay control module:
   ```cpp
   const uint8_t onoffPin = 2;  // Set your relay control pin here
   ```

3. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterOnOffPlugin.ino` sketch in the Arduino IDE.
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
Initial state: OFF
Matter Node is commissioned and connected to the network. Ready for use.
User Callback :: New Plugin State = ON
User Callback :: New Plugin State = OFF
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

Note: This example does not include button toggle functionality. The plugin is controlled exclusively via Matter app commands.

### State Persistence

The device saves the last known on/off state using the `Preferences` library. After a power cycle or restart:

- The device will restore to the last saved state (ON or OFF)
- Default state is OFF if no previous state was saved
- The Matter controller will be notified of the restored state
- The relay/LED will reflect the restored state

### Power Relay Integration

For production use with a power relay module:

1. **Connect the relay module** to your ESP32:
   - Relay VCC → ESP32 3.3 V or 5 V (check relay module specifications)
   - Relay GND → ESP32 GND
   - Relay IN → ESP32 GPIO pin (configured as `onoffPin`)

2. **Update the pin configuration** in the sketch:
   ```cpp
   const uint8_t onoffPin = 2;  // Your relay control pin
   ```

3. **Test the relay** by controlling it via Matter app - the relay should turn on/off accordingly

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an on/off switch/outlet in your Home app

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The plugin will appear in your Alexa app
6. You can control it using voice commands like "Alexa, turn on the plugin" or "Alexa, turn off the plugin"

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control it using voice commands or the app controls

## Code Structure

The MatterOnOffPlugin example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, relay/LED pin), configures Wi-Fi (if needed), initializes `Preferences` library, sets up the Matter plugin endpoint with the last saved state (defaults to OFF if not previously saved), registers the callback function, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, handles button input for factory reset, and allows the Matter stack to process events.

3. **Callbacks**:
   - `setPluginOnOff()`: Controls the physical relay/LED based on the on/off state, saves the state to `Preferences` for persistence, and prints the state change to Serial Monitor.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Relay/LED not responding**: Verify pin configurations and connections. For relay modules, ensure proper power supply and wiring
- **State not persisting**: Check that the `Preferences` library is properly initialized and that flash memory is not corrupted
- **Relay not switching**: For relay modules, verify the control signal voltage levels match your relay module requirements (some relays need 5 V, others work with 3.3 V)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter On/Off Plugin Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_on_off_plugin.html)

## License

This example is licensed under the Apache License, Version 2.0.
