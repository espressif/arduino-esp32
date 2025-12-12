# Matter Humidity Sensor Example

This example demonstrates how to create a Matter-compatible humidity sensor device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, sensor data reporting to smart home ecosystems, and automatic simulation of humidity readings.

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

- Matter protocol implementation for a humidity sensor device
- Support for both Wi-Fi and Thread(*) connectivity
- Humidity measurement reporting (0-100%)
- Automatic simulation of humidity readings (10% to 30% range)
- Periodic sensor updates every 5 seconds
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for factory reset (uses BOOT button by default)
- Optional: Connect a real humidity sensor (DHT11, DHT22, SHT31, etc.) and replace the simulation function

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
   To use a real humidity sensor, replace the `getSimulatedHumidity()` function with your sensor reading code. The function should return a float value representing humidity percentage (0-100%).

## Building and Flashing

1. Open the `MatterHumiditySensor.ino` sketch in the Arduino IDE.
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
Current Humidity is 95.00%
Current Humidity is 10.50%
Current Humidity is 11.00%
Current Humidity is 11.50%
...
Current Humidity is 30.00%
Current Humidity is 10.00%
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Sensor Simulation

The example includes a simulated humidity sensor that:

- Starts at 95% humidity (initial value)
- Cycles through 10% to 30% humidity range
- Increases in 0.5% steps
- Updates every 5 seconds
- Resets to 10% when reaching 30%

To use a real humidity sensor, replace the `getSimulatedHumidity()` function with your sensor library code. For example, with a DHT22:

```cpp
#include <DHT.h>
DHT dht(DHT_PIN, DHT22);

float getSimulatedHumidity() {
  return dht.readHumidity();
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
6. The device will appear as a humidity sensor in your Home app
7. You can monitor the humidity readings and set up automations based on humidity levels

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The humidity sensor will appear in your Alexa app
6. You can monitor humidity readings and create routines based on humidity levels

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can monitor humidity readings and create automations based on humidity levels

## Code Structure

The MatterHumiditySensor example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), sets up the Matter Humidity Sensor endpoint with initial value (95%), and waits for Matter commissioning.
2. **`loop()`**: Displays the current humidity value every 5 seconds, updates the sensor reading from the simulated hardware sensor, handles button input for factory reset, and allows the Matter stack to process events.
3. **`getSimulatedHumidity()`**: Simulates a hardware humidity sensor by cycling through values from 10% to 30% in 0.5% steps. Replace this function with your actual sensor reading code.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Humidity readings not updating**: Check that the sensor simulation function is being called correctly. For real sensors, verify sensor wiring and library initialization
<!-- vale Microsoft.Percentages = NO -->
- **Humidity values out of range**: Ensure humidity values are between 0-100%. The Matter protocol stores values as 1/100th of a percent internally
<!-- vale Microsoft.Percentages = YES -->
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Humidity Sensor Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_humidity_sensor.html)

## License

This example is licensed under the Apache License, Version 2.0.
