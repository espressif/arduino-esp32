# Matter Fan Example

This example demonstrates how to create a Matter-compatible fan device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, manual control using a physical button, and analog input for speed control.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | PWM Pin | Status |
| --- | ---- | ------ | ----------------- | ------- | ------ |
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

- Matter protocol implementation for a fan device
- Support for both Wi-Fi and Thread(*) connectivity
- On/Off control
- Speed control (0-100% in steps of 10%)
- Fan modes (OFF, ON, SMART, HIGH)
- Analog input for manual speed adjustment
- PWM output for DC motor control (simulated with RGB LED brightness)
- Button control for toggling fan and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- PWM-capable pin for DC motor control (uses RGB_BUILTIN or pin 2 by default)
- Analog input pin (A0) for speed control via potentiometer or similar
- User button for manual control (uses BOOT button by default)

## Pin Configuration

- **DC Motor PWM Pin**: Uses `RGB_BUILTIN` if defined, otherwise pin 2 (for controlling fan speed)
- **Analog Input Pin**: A0 (for reading speed control input, 0-1023 mapped to 10-100% speed)
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

2. **DC Motor PWM pin configuration** (if not using built-in RGB LED):
   ```cpp
   const uint8_t dcMotorPin = 2;  // Set your PWM pin here
   ```

3. **Analog input pin configuration** (optional):
   By default, analog pin A0 is used for speed control. You can change this if needed.
   ```cpp
   const uint8_t analogPin = A0;  // Set your analog pin here
   ```

4. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for the Fan On/Off manual control and factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterFan.ino` sketch in the Arduino IDE.
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
Fan State: Mode OFF | 0% speed.
User button released. Setting the Fan ON.
Fan State: Mode ON | 50% speed.
Fan set to SMART mode -- speed percentage will go to 50%
Fan State: Mode SMART | 50% speed.
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Toggle fan on/off
- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Analog Speed Control

The analog input pin (A0) allows manual speed adjustment:

- Connect a potentiometer or similar analog input to pin A0
- Analog values (0-1023) are mapped to speed percentages (10-100%) in steps of 10%
- The speed automatically updates when the analog input changes
- Speed changes are synchronized with the Matter controller

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a fan in your Home app
7. You can control on/off, speed (0-100%), and fan modes (OFF, ON, SMART, HIGH)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The fan will appear in your Alexa app
6. You can control speed and modes using voice commands like "Alexa, set fan to 50 percent" or "Alexa, set fan to high"

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control speed and modes using voice commands or the app controls

## Code Structure

The MatterFan example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, analog input, PWM output), configures Wi-Fi (if needed), sets up the Matter Fan endpoint with initial state (OFF, 0% speed), and registers callbacks for state changes.
2. **`loop()`**: Checks the Matter commissioning state, handles button input for toggling the fan and factory reset, reads analog input to adjust fan speed, and allows the Matter stack to process events.
3. **Callbacks**:
   - `onChangeSpeedPercent()`: Handles speed percentage changes (0% to 100%). Automatically turns fan on/off based on speed.
   - `onChangeMode()`: Handles fan mode changes (OFF, ON, SMART, HIGH). Automatically sets speed to 50% when switching from OFF to another mode.
   - `onChange()`: Generic callback that controls the DC motor via PWM and reports the current state.
   - `fanDCMotorDrive()`: Drives the DC motor (or simulates it with RGB LED brightness) based on fan state and speed.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Fan speed not responding**: Verify PWM pin configuration and connections. For DC motor control, ensure the pin supports PWM output
- **Analog input not working**: Verify analog pin configuration and that the input voltage is within the ESP32's ADC range (0-3.3V typically)
- **Speed changes not synchronized**: The analog input is read continuously, and speed updates only when the value changes by a step (0-9 steps mapped to 10-100%)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Fan Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_fan.html)

## License

This example is licensed under the Apache License, Version 2.0.
