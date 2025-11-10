# Matter Commission Test Example

This example demonstrates how to test Matter commissioning functionality using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device connection to smart home ecosystems, and automatic decommissioning after a 30-second delay for continuous testing cycles.

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
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Matter protocol implementation for an on/off light device
- Support for both Wi-Fi and Thread(*) connectivity
- Matter commissioning via QR code or manual pairing code
- Automatic decommissioning after 30 seconds for continuous testing
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
- Simple test tool for validating Matter commissioning workflows
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)

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

## Building and Flashing

1. Open the `MatterCommissionTest.ino` sketch in the Arduino IDE.
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
Matter Fabric not commissioned yet. Waiting for commissioning.
Matter Fabric not commissioned yet. Waiting for commissioning.
...
Matter Node is commissioned and connected to the network.
====> Decommissioning in 30 seconds. <====
Matter Node is decommissioned. Commissioning widget shall start over.

Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
...
```

## Using the Device

### Test Cycle

The device operates in a continuous test cycle:

1. **Commissioning Phase**: The device waits for Matter commissioning. It displays the manual pairing code and QR code URL in the Serial Monitor.
2. **Commissioned Phase**: Once commissioned, the device is connected to the Matter network and ready for use.
3. **Automatic Decommissioning**: After 30 seconds, the device automatically decommissions itself.
4. **Repeat**: The cycle repeats, allowing you to test the commissioning process multiple times.

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device during each test cycle.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an on/off light in your Home app
7. After 30 seconds, the device will automatically decommission and the cycle will repeat

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The light will appear in your Alexa app
6. After 30 seconds, the device will automatically decommission and the cycle will repeat

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. After 30 seconds, the device will automatically decommission and the cycle will repeat

## Code Structure

The MatterCommissionTest example consists of the following main components:

1. **`setup()`**: Configures Wi-Fi (if needed), initializes the Matter On/Off Light endpoint, and starts the Matter stack.
2. **`loop()`**: Checks the Matter commissioning state, displays pairing information when not commissioned, waits for commissioning, and then automatically decommissions after 30 seconds to repeat the cycle.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Failed to commission**: Try waiting for the next cycle after decommissioning. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection
- **Device keeps decommissioning**: This is expected behavior - the device automatically decommissions after 30 seconds to allow continuous testing

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)

## License

This example is licensed under the Apache License, Version 2.0.
