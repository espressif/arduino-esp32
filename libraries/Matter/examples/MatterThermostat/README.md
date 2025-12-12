# Matter Thermostat Example

This example demonstrates how to create a Matter-compatible thermostat device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, thermostat control via smart home ecosystems, temperature setpoint management, and simulated heating/cooling systems with automatic temperature regulation.

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

- Matter protocol implementation for a thermostat device
- Support for both Wi-Fi and Thread(*) connectivity
- Multiple thermostat modes: OFF, HEAT, COOL, AUTO
- Heating and cooling setpoint control
- Automatic temperature regulation in AUTO mode
- Simulated heating/cooling systems with temperature changes
- Serial input for manual temperature setting
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for factory reset (uses BOOT button by default)
- Optional: Connect a real temperature sensor and replace the simulation function

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
   To use a real temperature sensor, replace the `getSimulatedTemperature()` function with your sensor reading code. The function should return a float value representing temperature in Celsius.

## Building and Flashing

1. Open the `MatterThermostat.ino` sketch in the Arduino IDE.
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

Initial Setpoints are 23.0C to 20.0C with a minimum 2.5C difference
Auto mode is ON. Initial Temperature of 12.5C
Local Temperature Sensor will be simulated every 10 seconds and changed by a simulated heater and cooler to move in between setpoints.
Current Local Temperature is 12.5C
	Thermostat Mode: AUTO >>> Heater is ON -- Cooler is OFF
Current Local Temperature is 13.0C
	Thermostat Mode: AUTO >>> Heater is ON -- Cooler is OFF
...
Current Local Temperature is 20.0C
	Thermostat Mode: AUTO >>> Heater is OFF -- Cooler is OFF
Current Local Temperature is 23.0C
	Thermostat Mode: AUTO >>> Heater is OFF -- Cooler is ON
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Serial Input

You can manually set the temperature by typing a value in the Serial Monitor:

1. Open Serial Monitor at 115200 baud
2. Type a temperature value between -50°C and 50°C
3. Press Enter
4. The thermostat will update the local temperature reading

Example:
```
15.5
New Temperature is 15.5C
```

### Thermostat Modes

The thermostat supports four operating modes:

- **OFF**: Heating and cooling systems are turned off
- **HEAT**: Only heating system is active. Turns off when temperature exceeds heating setpoint
- **COOL**: Only cooling system is active. Turns off when temperature falls below cooling setpoint
- **AUTO**: Automatically switches between heating and cooling to maintain temperature between setpoints

### Setpoints

- **Heating Setpoint**: Target temperature for heating (default: 23.0°C)
- **Cooling Setpoint**: Target temperature for cooling (default: 20.0°C)
- **Deadband**: Minimum difference between heating and cooling setpoints (2.5°C required in AUTO mode)

### Temperature Simulation

The example includes a simulated heating/cooling system:

- **Heating**: Temperature increases by 0.5°C every 10 seconds when heating is active
- **Cooling**: Temperature decreases by 0.5°C every 10 seconds when cooling is active
- **No heating/cooling**: Temperature remains stable

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a thermostat in your Home app
7. You can control the mode (OFF, HEAT, COOL, AUTO) and adjust heating/cooling setpoints
8. Monitor the current temperature and set up automations based on temperature

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The thermostat will appear in your Alexa app
6. You can control the mode and setpoints using voice commands or the app
7. Create routines based on temperature changes

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control the mode and setpoints using voice commands or the app
7. Create automations based on temperature

## Code Structure

The MatterThermostat example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), sets up the Matter Thermostat endpoint with cooling/heating sequence of operation and AUTO mode enabled, sets initial setpoints (heating: 23.0°C, cooling: 20.0°C) and initial temperature (12.5°C), and waits for Matter commissioning.

2. **`loop()`**: Reads serial input for manual temperature setting, simulates heating/cooling systems and temperature changes every 10 seconds, controls heating/cooling based on thermostat mode and setpoints, handles button input for factory reset, and allows the Matter stack to process events.

3. **`getSimulatedTemperature()`**: Simulates temperature changes based on heating/cooling state. Temperature increases when heating is active, decreases when cooling is active. Replace this function with your actual sensor reading code.

4. **`readSerialForNewTemperature()`**: Reads temperature values from Serial Monitor input, validates the range (-50°C to 50°C), and updates the thermostat's local temperature.

5. **Thermostat Control Logic**:
   - **OFF mode**: Both heating and cooling are disabled
   - **AUTO mode**: Automatically switches between heating and cooling to maintain temperature between setpoints
   - **HEAT mode**: Activates heating until temperature exceeds heating setpoint
   - **COOL mode**: Activates cooling until temperature falls below cooling setpoint

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Temperature not updating**: Check that the simulation function is being called correctly. For real sensors, verify sensor wiring and library initialization
- **Heating/cooling not responding**: Verify that the thermostat mode is set correctly and that setpoints are properly configured
- **Setpoints not working**: Ensure cooling setpoint is at least 2.5°C lower than heating setpoint in AUTO mode
- **Serial input not working**: Make sure Serial Monitor is set to 115200 baud and "No line ending" or "Newline" is selected
- **Invalid temperature values**: Temperature must be between -50°C and 50°C. The Matter protocol stores values as int16_t internally (1/100th of a degree Celsius)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Thermostat Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_thermostat.html)

## License

This example is licensed under the Apache License, Version 2.0.
