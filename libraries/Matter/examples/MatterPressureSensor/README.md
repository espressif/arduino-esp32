# Matter Pressure Sensor Example

This example demonstrates how to create a Matter-compatible pressure sensor device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, sensor data reporting to smart home ecosystems, and automatic simulation of pressure readings.

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

- Matter protocol implementation for a pressure sensor device
- Support for both Wi-Fi and Thread(*) connectivity
- Pressure measurement reporting in hectopascals (hPa)
- Automatic simulation of pressure readings (950-1100 hPa range)
- Periodic sensor updates every 5 seconds
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for factory reset (uses BOOT button by default)
- Optional: Connect a real pressure sensor (BMP280, BME280, BMP388, etc.) and replace the simulation function

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

3. **Real sensor integration** (optional):
   To use a real pressure sensor, replace the `getSimulatedPressure()` function with your sensor reading code. The function should return a float value representing pressure in hectopascals (hPa).

## Building and Flashing

1. Open the `MatterPressureSensor.ino` sketch in the Arduino IDE.
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
Current Pressure is 900.00hPa
Current Pressure is 950.00hPa
Current Pressure is 960.00hPa
...
Current Pressure is 1100.00hPa
Current Pressure is 950.00hPa
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Sensor Simulation

The example includes a simulated pressure sensor that:

- Starts at 900 hPa (initial value)
- Cycles through 950 to 1100 hPa range
- Increases in 10 hPa steps
- Updates every 5 seconds
- Resets to 950 hPa when reaching 1100 hPa

To use a real pressure sensor, replace the `getSimulatedPressure()` function with your sensor library code. For example, with a BMP280:

```cpp
#include <Adafruit_BMP280.h>
Adafruit_BMP280 bmp;

float getSimulatedPressure() {
  return bmp.readPressure() / 100.0;  // Convert Pa to hPa
}
```

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a pressure sensor in your Home app
7. You can monitor the pressure readings and set up automations based on pressure levels (e.g., weather monitoring)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The pressure sensor will appear in your Alexa app
6. You can monitor pressure readings and create routines based on pressure changes

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can monitor pressure readings and create automations based on pressure changes

## Code Structure

The MatterPressureSensor example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), sets up the Matter Pressure Sensor endpoint with initial value (900 hPa), and waits for Matter commissioning.

2. **`loop()`**: Displays the current pressure value every 5 seconds, updates the sensor reading from the simulated hardware sensor, handles button input for factory reset, and allows the Matter stack to process events.

3. **`getSimulatedPressure()`**: Simulates a hardware pressure sensor by cycling through values from 950 to 1100 hPa in 10 hPa steps. Replace this function with your actual sensor reading code.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Pressure readings not updating**: Check that the sensor simulation function is being called correctly. For real sensors, verify sensor wiring and library initialization
- **Pressure values out of range**: Ensure pressure values are in hectopascals (hPa). The Matter protocol stores values as uint16_t internally. Typical atmospheric pressure ranges from 950-1050 hPa at sea level
- **State not changing**: The simulated sensor increases by 10 hPa every 5 seconds. If you're using a real sensor, ensure it's properly connected and reading correctly
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Pressure Sensor Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_pressure_sensor.html)

## License

This example is licensed under the Apache License, Version 2.0.
