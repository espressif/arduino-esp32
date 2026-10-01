# Matter Water Valve Example

This example demonstrates how to create a Matter-compatible water valve device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and manual control using a physical button - suitable for irrigation valves, main water shutoff valves, or appliance water supply valves.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | LED | Status |
| --- | ---- | ------ | ----------------- | --- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Required | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-C5 | ❌ | ✅ | ✅ | Required | Supported (Thread only) |
| ESP32-C6 | ✅ | ❌ | ✅ | Required | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Required | Supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project using Arduino as an IDF Component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Wi-Fi 2.4 GHz and 5 GHz support, the ESP32 Arduino Matter Library has been pre compiled using Thread only. In order to configure it for Wi-Fi operation it is necessary to build the project using Arduino as an ESP-IDF component and disable Thread network, keeping only Wi-Fi station.

## Features

- Matter protocol implementation for a water valve device (device type 0x0042, Valve Configuration and Control cluster)
- Support for both Wi-Fi and Thread(*) connectivity
- Open (indefinitely or for a set duration, with automatic closing) and Close control
- Automatic countdown of the `RemainingDuration` attribute for timed open operations - handled internally, no polling loop needed in the sketch
- Valve fault reporting
- Button control for manual open/close and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- LED connected to GPIO pin (or using built-in LED) for visual feedback - replace with a relay/solenoid driver for a real valve
- User button for manual control (uses BOOT button by default)

## Pin Configuration

- **LED**: Uses `LED_BUILTIN` if defined, otherwise pin 2
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
   By default, the `BOOT` button (GPIO 0) is used for manual open/close control and factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

4. **Open duration** (optional):
   The example opens the valve for a fixed duration when toggled manually. Adjust it to fit your use case, or call `WaterValve.open()` with no argument to open indefinitely.
   ```cpp
   const uint32_t openDurationSeconds = 10;  // Set your desired open duration here
   ```

## Building and Flashing

1. Open the `MatterWaterValve.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

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
User button released. Opening the water valve!
User Callback :: Opening the water valve
Water valve remaining duration: 10 s
Water valve remaining duration: 9 s
...
Water valve remaining duration: 1 s
User Callback :: Closing the water valve
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Toggle the valve open (for `openDurationSeconds`) or closed
- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a valve in your Home app

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The valve will appear in your Alexa app
6. You can control it using voice commands like "Alexa, turn on the water valve" or "Alexa, turn off the water valve"

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control it using voice commands or the app controls

## Code Structure

The MatterWaterValve example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, LED), configures Wi-Fi (if needed), sets up the Matter Water Valve endpoint, registers the `onOpen()`/`onClose()` callbacks, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, prints the remaining duration of a timed open operation as it counts down, handles button input for toggling the valve and factory reset, and allows the Matter stack to process events.

3. **Callbacks**:
   - `onValveOpen()`: Called whenever the valve is commanded open, either by a Matter controller or locally via `open()`. Drives the physical actuator (the LED, in this example) and returns success/failure to the Matter core.
   - `onValveClose()`: Called whenever the valve is commanded closed, either by a Matter controller, locally via `close()`, or automatically when a timed open operation elapses.

For a production water valve, replace the LED control in `onValveOpen()`/`onValveClose()` with your actual relay/solenoid driver code.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **LED not responding**: Verify pin configurations and connections
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **Button not toggling valve**: Ensure the button is properly connected and the debounce time is appropriate. Check Serial Monitor for "User button released" messages
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)

## License

This example is licensed under the Apache License, Version 2.0.
