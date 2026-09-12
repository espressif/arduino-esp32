# Matter Temperature Controlled Cabinet Example (Temperature Level Mode)

This example demonstrates how to create a Matter-compatible temperature controlled cabinet device using the **temperature_level** feature mode. This mode provides temperature control using predefined levels (Off / Low / Medium / High / Maximum) rather than a numeric setpoint.

**Important:** The `temperature_number` and `temperature_level` features are **mutually exclusive**. Only one can be enabled at a time. See the [MatterTemperatureControlledCabinet](../MatterTemperatureControlledCabinet) example for temperature setpoint control mode.

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
- **ESP32-C5:** Wi-Fi + CHIPoBLE by default (Tools → Matter Network → Wi-Fi). Thread is Tools → Matter Network → Thread.

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (ESP32-C5 / ESP32-C6 / ESP32-H2): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (ESP32-C5 / ESP32-C6 / ESP32-H2): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Matter protocol implementation for a temperature controlled cabinet device
- Default network and CHIPoBLE as in the Supported Targets table (ESP32-C6 dual-stack uses Wi-Fi unless you call `selectNetwork()`)
- Temperature level control with an array of application uint8 values (`10`, `20`, `30`, `40`, `50`)
- Those values are advertised to hubs as Matter string labels (`"Off"` … `"Max"`); the hub writes a list **index** (`0`…`4`)
- Serial prints the Arduino value, the Matter index, and the hub label so the mapping is visible
- Up to 16 predefined temperature levels
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

## Use Case

Use this mode when you need simple preset-based temperature control rather than a precise setpoint. The sketch uses `10=Off`, `20=Low`, `30=Medium`, `40=High`, `50=Max` so the application value is not the same number as the Matter list index. The hub UI shows `"Off"` … `"Max"`.

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
   - `WiFi` (only for ESP32 and ESP32-S2)

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
   Adjust the supported levels array and initial **value** (must appear in the array):
   ```cpp
   uint8_t supportedLevels[] = {10, 20, 30, 40, 50};  // Arduino values
   const char *supportedLevelLabels[] = {"Off", "Low", "Medium", "High", "Max"};
   const uint16_t levelCount = sizeof(supportedLevels) / sizeof(supportedLevels[0]);
   const uint8_t initialLevel = 30;  // Medium — Matter stores index 2
   TemperatureCabinet.begin(supportedLevels, supportedLevelLabels, levelCount, initialLevel);
   // The uint8 array is copied internally; it does not need to stay valid after begin() returns.
   // Label pointers are not copied; these string literals must outlive the endpoint.
   // setSelectedTemperatureLevel() / getSelectedTemperatureLevel() use these uint8 values.
   // Controllers write SelectedTemperatureLevel as an index into the advertised string list.
   ```

## Building and Flashing

1. Open the `MatterTemperatureControlledCabinetLevels.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. Wi-Fi connection messages appear only on ESP32 and ESP32-S2. CHIPoBLE targets get the operational network from the hub (Wi-Fi, or Thread on ESP32-C5 / ESP32-C6 / ESP32-H2). You should see output similar to the following:

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
  Arduino getSelectedTemperatureLevel() = 30
  Matter SelectedTemperatureLevel index = 2
  SupportedTemperatureLevels count = 5
  List [index]=value("hub label"): [0]=10("Off"), [1]=20("Low"), [2]=30("Medium"), [3]=40("High"), [4]=50("Max")
  Hub SetTemperature writes an index (0..4). Arduino setters/getters use the uint8 value.
Temperature level updated: value 40, Matter index 3, hub label "High"
*** Temperature level 30 reached/overpassed while increasing ***
Temperature level updated: value 50, Matter index 4, hub label "Max"
Temperature level updated: value 40, Matter index 3, hub label "High"
Temperature level updated: value 30, Matter index 2, hub label "Medium"
*** Temperature level 30 reached/overpassed while decreasing ***
Temperature level updated: value 20, Matter index 1, hub label "Low"
...
Current temperature level: value 30, Matter index 2, hub label "Medium"
```

On the hub, `SupportedTemperatureLevels` is the string list `"Off"`, `"Low"`, `"Medium"`, `"High"`, `"Max"`. Choosing the third entry writes index `2`; `getSelectedTemperatureLevel()` still returns `30`.

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Smart Home Integration

Use a Matter-compatible hub (like a Home Assistant server, Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Home Assistant

1. Open Home Assistant
2. Go to Settings > Devices & services > Add integration > Matter
3. Scan the QR code from the Serial Monitor, or enter the manual pairing code
4. Follow the prompts to complete setup

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a temperature controlled cabinet in your Home app
7. You can select from the advertised string labels (`"Off"` … `"Max"` in this sketch)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The temperature controlled cabinet will appear in your Alexa app
6. You can select from the advertised string labels and set up routines

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. The temperature controlled cabinet will appear in your Google Home app
7. You can select from the advertised string labels (`"Off"` … `"Max"` in this sketch)

## Code Structure

The MatterTemperatureControlledCabinetLevels example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), sets up the Matter Temperature Controlled Cabinet endpoint with temperature level configuration, and waits for Matter commissioning.

2. **`loop()`**:
   - **Dynamic Level Updates**: Automatically cycles through all supported temperature levels every 1 second in both directions (increasing and decreasing). This demonstrates the temperature level control functionality and allows Matter controllers to observe real-time changes.
   - **Level Reached Detection**: Monitors when the initial level is reached or overpassed in each direction and prints a notification message once per direction.
   - Periodically prints the current temperature level (every 5 seconds)
   - Handles button input for factory reset

3. **Helper Functions**:
   - `indexOfLevel()` / `printSupportedLevels()` / `printLevelMapping()`: Map an Arduino uint8 value to the Matter list index and hub string label
   - `initLevelControl()`: Initializes the level control state from the current selected level
   - `checkLevelReached()`: Checks and logs when the initial level is reached/overpassed
   - `updateTemperatureLevel()`: Updates the temperature level with cycling logic and boundary detection
   - `printLevelStatus()`: Prints the current value, Matter index, and hub label
   - `handleButtonPress()`: Handles button press detection and factory reset functionality

## API Usage

The example demonstrates the following API methods:

- `begin(supportedLevels, labels, levelCount, selectedLevel)` — `selectedLevel` is a **value from the array**, not a Matter index; `labels` are hub-visible names (pointers not copied)
- `getSelectedTemperatureLevel()` — returns that uint8 value (here `10`…`50`)
- `setSelectedTemperatureLevel(level)` — takes the same uint8 value; Matter stores the matching index
- `getSupportedTemperatureLevelsCount()` — number of advertised string labels

`setSupportedTemperatureLevels()` is not used after `begin()` in this sketch. If you call it later, the current selected **value** must still appear in the new array. Pass labels the same way as `begin()`, or omit them to advertise decimals.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Temperature level not updating**: Check Serial Monitor output to verify level changes are being processed
- **Invalid level error**: `begin()` / `setSelectedTemperatureLevel()` take a value from `supportedLevels[]` (here `10`…`50`), not the hub index (`0`…`4`)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection
- **Wrong mode error**: Remember that temperature_number and temperature_level modes are mutually exclusive. Make sure you're using the correct example and API methods for temperature level mode

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Temperature Controlled Cabinet Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_temperature_controlled_cabinet.html)

## License

This example is licensed under the Apache License, Version 2.0.
