# Matter Enhanced Color Light Example

This example demonstrates how to create a Matter-compatible enhanced color light device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and manual control using a physical button. The enhanced color light provides color temperature and brightness in addition to HSV/XY color.

Use this example when the endpoint should include **on/off, brightness, color, and color temperature**. For RGB only (no color temperature), use the [MatterColorLight](../MatterColorLight) example.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       | RGB LED  |
| -------- | ---------------------- | -------- | ---------------------- | -------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) | Required |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         | Required |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C5 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI) | Required |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         | Required |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `WiFi.begin(ssid, password)`.
- **ESP32-C6:** prebuild is dual-stack. Without `selectNetwork()` this sketch uses **Wi-Fi + CHIPoBLE**. Thread stays unused.
- **ESP32-H2:** Thread + CHIPoBLE (no Wi-Fi).
- **ESP32-C5:** Wi-Fi + CHIPoBLE. Thread is not in that prebuild.

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (C6/H2): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (C6/H2): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Matter protocol implementation for an enhanced color light device
- Default network and CHIPoBLE as in the Supported Targets table (C6 dual-stack uses Wi-Fi unless you call `selectNetwork()`)
- RGB color control with HSV color model (hue 0-254, saturation 0-254, value 0-254)
- Color temperature control (warm to cool white; higher mireds are warmer)
- Brightness control (Arduino API 0-255; Matter CurrentLevel is 1-254)
- State persistence using `Preferences` library
- Button control for toggling light and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple HomeKit, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- RGB LED connected to GPIO pins (or using built-in RGB LED)
- User button for manual control (uses BOOT button by default)

## Pin Configuration

- **RGB LED**: Uses `RGB_BUILTIN` if defined, otherwise pin 2
- **Button**: Uses `BOOT_PIN` by default

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `Preferences`
   - `WiFi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi credentials** (if not using BLE commissioning - mandatory for ESP32 | ESP32-S2):
   ```cpp
   const char *ssid = "your-ssid";         // Change to your Wi-Fi SSID
   const char *password = "your-password"; // Change to your Wi-Fi password
   ```

2. **LED pin configuration** (if not using built-in RGB LED):
   ```cpp
   const uint8_t ledPin = 2;  // Set your RGB LED pin here
   ```

3. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for the Light On/Off manual control. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterEnhancedColorLight.ino` sketch in the Arduino IDE.
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
Initial state: ON | RGB Color: (255,255,255)
Matter Node is commissioned and connected to the network. Ready for use.
Light OnOff changed to ON
Light Color Temperature changed to 370
Light brightness changed to 128
Light HSV Color changed to (84,254,254)
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Toggle light on/off
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
6. The device will appear as an enhanced color light in your Home app
7. You can control RGB color, color temperature (warm/cool white), and brightness

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The enhanced color light will appear in your Alexa app
6. You can control color, color temperature, and brightness using voice commands or the app

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control color, color temperature, and brightness using voice commands or the app controls

## Code Structure

The MatterEnhancedColorLight example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, LED), configures Wi-Fi (if needed), sets up the Matter endpoint, restores the last known state (on/off and HSV color) from `Preferences`, and registers callbacks for state changes.
2. **`loop()`**: Checks the Matter commissioning state, handles button input for toggling the light and factory reset, and allows the Matter stack to process events.
3. **Callbacks**:
   - `setLightState()`: Controls the physical RGB LED with state, HSV color, brightness, and color temperature parameters.
   - `onChangeOnOff()`: Handles on/off state changes.
   - `onChangeColorHSV()`: Handles HSV color changes (Hue and Saturation).
   - `onChangeBrightness()`: Handles brightness level changes (0-255, maps to HSV Value).
   - `onChangeColorTemperature()`: Handles color temperature changes (warm to cool white).

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **RGB LED not responding**: Verify pin configurations and connections
- **Color temperature not working**: Verify that the color temperature callback is properly handling HSV conversion
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Enhanced Color Light Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_enhanced_color_light.html)

## License

This example is licensed under the Apache License, Version 2.0.
