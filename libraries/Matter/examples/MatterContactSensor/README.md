# Matter Contact Sensor Example

This example demonstrates how to create a Matter-compatible contact sensor device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, manual control using a physical button, and automatic simulation of contact state changes.

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

- Matter protocol implementation for a contact sensor device
- Support for both Wi-Fi and Thread(*) connectivity
- Contact state indication using LED (ON = Closed, OFF = Open)
- Automatic simulation of contact state changes every 20 seconds
- Button control for toggling contact state and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- LED connected to GPIO pins (or using built-in LED) to indicate contact state
- User button for manual control (uses BOOT button by default)

## Pin Configuration

- **LED**: Uses `RGB_BUILTIN` if defined, otherwise pin 2
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
   By default, the `BOOT` button (GPIO 0) is used for the Contact Sensor state toggle and factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterContactSensor.ino` sketch in the Arduino IDE.
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
Matter Node is commissioned and connected to the network. Ready for use.
User button released. Setting the Contact Sensor to Closed.
User button released. Setting the Contact Sensor to Open.
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Toggle contact sensor state (Open/Closed)
- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Automatic Simulation

The contact sensor state automatically toggles every 20 seconds to simulate a real contact sensor (such as a door or window sensor). The LED will reflect the current state:
- **LED ON**: Contact sensor is Closed
- **LED OFF**: Contact sensor is Open

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a contact sensor in your Home app
7. You can monitor the contact state (Open/Closed) and receive notifications when the state changes

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The contact sensor will appear in your Alexa app
6. You can monitor the contact state and set up routines based on state changes

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. The contact sensor will appear in your Google Home app

## Code Structure

The MatterContactSensor example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, LED), configures Wi-Fi (if needed), sets up the Matter Contact Sensor endpoint with initial state (Open), and waits for Matter commissioning.
2. **`loop()`**: Handles button input for toggling contact state and factory reset, and automatically simulates contact state changes every 20 seconds.
3. **`simulatedHWContactSensor()`**: Simulates a hardware contact sensor by toggling the contact state every 20 seconds.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **LED not responding**: Verify pin configurations and connections
- **Contact sensor state not updating**: Check Serial Monitor output to verify state changes are being processed
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Contact Sensor Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_contact_sensor.html)

## License

This example is licensed under the Apache License, Version 2.0.
