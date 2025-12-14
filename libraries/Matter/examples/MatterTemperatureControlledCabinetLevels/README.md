# Matter Temperature Controlled Cabinet Example (Temperature Level Mode)

This example demonstrates how to create a Matter-compatible temperature controlled cabinet device using the **temperature_level** feature mode. This mode provides temperature control using predefined levels (e.g., Off, Low, Medium, High, Maximum) rather than precise temperature setpoint values.

**Important:** The `temperature_number` and `temperature_level` features are **mutually exclusive**. Only one can be enabled at a time. See `MatterTemperatureControlledCabinet` example for temperature setpoint control mode.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | Status |
| --- | ---- | ------ | ----------------- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C5 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C6 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Matter protocol implementation for a temperature controlled cabinet device
- Support for both Wi-Fi and Thread(*) connectivity
- Temperature level control with array of supported levels
- Up to 16 predefined temperature levels
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Use Case

Use this mode when you need simple preset-based temperature control (e.g., 0=Off, 1=Low, 2=Medium, 3=High, 4=Maximum) rather than precise temperature values. This is ideal for devices where users select from predefined temperature presets rather than setting exact temperatures.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for factory reset (uses BOOT button by default)
- Optional: Connect temperature control hardware (relays, heaters, coolers, etc.) to implement actual temperature control

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
   By default, the `BOOT` button (GPIO 0) is used for factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

3. **Temperature levels configuration** (optional):
   Adjust the supported levels array and initial level in the sketch:
   ```cpp
   uint8_t supportedLevels[] = {0, 1, 2, 3, 4};  // Define your levels
   const uint16_t levelCount = sizeof(supportedLevels) / sizeof(supportedLevels[0]);
   const uint8_t initialLevel = 2;  // Initial selected level
   TemperatureCabinet.begin(supportedLevels, levelCount, initialLevel);
   // Note: The array is copied internally, so it doesn't need to remain valid after begin() returns
   ```

## Building and Flashing

1. Open the `MatterTemperatureControlledCabinetLevels.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. The Wi-Fi connection messages will be displayed only for ESP32 and ESP32-S2. Other targets will use Matter CHIPoBLE to automatically setup the IP Network. You should see output similar to the following:

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

Temperature Controlled Cabinet Configuration (Temperature Level Mode):
  Selected Level: 2
  Supported Levels Count: 5
  Supported Levels: 0, 1, 2, 3, 4
Temperature level updated to: 3 (Supported Levels: 0, 1, 2, 3, 4)
*** Temperature level 2 reached/overpassed while increasing ***
Temperature level updated to: 4 (Supported Levels: 0, 1, 2, 3, 4)
Temperature level updated to: 3 (Supported Levels: 0, 1, 2, 3, 4)
Temperature level updated to: 2 (Supported Levels: 0, 1, 2, 3, 4)
*** Temperature level 2 reached/overpassed while decreasing ***
Temperature level updated to: 1 (Supported Levels: 0, 1, 2, 3, 4)
...
Current Temperature Level: 2 (Supported Levels: 0, 1, 2, 3, 4)
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a temperature controlled cabinet in your Home app
7. You can select from the available temperature levels

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The temperature controlled cabinet will appear in your Alexa app
6. You can select temperature levels and set up routines

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. The temperature controlled cabinet will appear in your Google Home app
7. You can select from available temperature levels

## Code Structure

The MatterTemperatureControlledCabinetLevels example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), sets up the Matter Temperature Controlled Cabinet endpoint with temperature level configuration, and waits for Matter commissioning.

2. **`loop()`**:
   - **Dynamic Level Updates**: Automatically cycles through all supported temperature levels every 1 second in both directions (increasing and decreasing). This demonstrates the temperature level control functionality and allows Matter controllers to observe real-time changes.
   - **Level Reached Detection**: Monitors when the initial level is reached or overpassed in each direction and prints a notification message once per direction.
   - Periodically prints the current temperature level (every 5 seconds)
   - Handles button input for factory reset

3. **Helper Functions**:
   - `initLevelControl()`: Initializes the level control state from the current selected level
   - `checkLevelReached()`: Checks and logs when the initial level is reached/overpassed
   - `updateTemperatureLevel()`: Updates the temperature level with cycling logic and boundary detection
   - `printLevelStatus()`: Prints the current level status
   - `handleButtonPress()`: Handles button press detection and factory reset functionality

## API Usage

The example demonstrates the following API methods:

- `begin(supportedLevels, levelCount, selectedLevel)` - Initialize the cabinet with temperature levels
- `getSelectedTemperatureLevel()` - Get current selected temperature level
- `setSelectedTemperatureLevel(level)` - Set selected temperature level
- `getSupportedTemperatureLevelsCount()` - Get count of supported levels
- `setSupportedTemperatureLevels(levels, count)` - Set supported temperature levels array

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Temperature level not updating**: Check Serial Monitor output to verify level changes are being processed
- **Invalid level error**: Ensure the selected level is in the supported levels array
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection
- **Wrong mode error**: Remember that temperature_number and temperature_level modes are mutually exclusive. Make sure you're using the correct example and API methods for temperature level mode

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Temperature Controlled Cabinet Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_temperature_controlled_cabinet.html)

## License

This example is licensed under the Apache License, Version 2.0.
