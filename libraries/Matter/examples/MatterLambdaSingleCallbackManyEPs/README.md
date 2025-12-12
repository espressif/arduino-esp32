# Matter Lambda Single Callback Many Endpoints Example

This example demonstrates how to create multiple Matter endpoints in a single node using a shared lambda function callback with capture in an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, multiple endpoint management, and efficient callback handling using C++ lambda functions with capture variables.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | GPIO Pins | Status |
| --- | ---- | ------ | ----------------- | --------- | ------ |
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

- Matter protocol implementation with multiple endpoints in a single node
- Six on/off light endpoints sharing a single callback function
- Lambda function with capture variable for efficient endpoint identification
- Support for both Wi-Fi and Thread(*) connectivity
- Each endpoint has a unique GPIO pin and friendly name
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Optional: Six LEDs connected to GPIO pins (2, 4, 6, 8, 10, 12) for visual feedback

## Pin Configuration

By default, the example uses six GPIO pins for the on/off lights:
- **Light 1 (Room 1)**: GPIO 2
- **Light 2 (Room 2)**: GPIO 4
- **Light 3 (Room 3)**: GPIO 6
- **Light 4 (Room 4)**: GPIO 8
- **Light 5 (Room 5)**: GPIO 10
- **Light 6 (Room 6)**: GPIO 12

You can modify the `lightPins` array to match your hardware configuration:

```cpp
uint8_t lightPins[MAX_LIGHT_NUMBER] = {2, 4, 6, 8, 10, 12};
```

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `Preferences`
   - `Wi-Fi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi credentials** (if not using BLE commissioning - mandatory for ESP32 | ESP32-S2):
   ```cpp
   const char *ssid = "your-ssid";         // Change to your Wi-Fi SSID
   const char *password = "your-password"; // Change to your Wi-Fi password
   ```

2. **GPIO pin configuration** (optional):
   Modify the `lightPins` array to match your hardware:
   ```cpp
   uint8_t lightPins[MAX_LIGHT_NUMBER] = {2, 4, 6, 8, 10, 12};
   ```

3. **Light names** (optional):
   Modify the `lightName` array to customize the friendly names:
   ```cpp
   const char *lightName[MAX_LIGHT_NUMBER] = {
     "Room 1", "Room 2", "Room 3", "Room 4", "Room 5", "Room 6",
   };
   ```

4. **Number of endpoints** (optional):
   Change `MAX_LIGHT_NUMBER` to create more or fewer endpoints:
   ```cpp
   const uint8_t MAX_LIGHT_NUMBER = 6;
   ```

## Building and Flashing

1. Open the `MatterLambdaSingleCallbackManyEPs.ino` sketch in the Arduino IDE.
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
Matter App Control: 'Room 1' (OnOffLight[0], Endpoint 1, GPIO 2) changed to: OFF
Matter App Control: 'Room 1' (OnOffLight[0], Endpoint 1, GPIO 2) changed to: ON
Matter App Control: 'Room 5' (OnOffLight[4], Endpoint 5, GPIO 10) changed to: ON
Matter App Control: 'Room 2' (OnOffLight[1], Endpoint 2, GPIO 4) changed to: ON
```

## Using the Device

### Lambda Function with Capture

This example demonstrates how to use C++ lambda functions with capture variables to efficiently handle multiple endpoints with a single callback function. The lambda capture `[i]` allows the callback to identify which endpoint triggered the event:

```cpp
OnOffLight[i].onChangeOnOff([i](bool state) -> bool {
  Serial.printf(
    "Matter App Control: '%s' (OnOffLight[%d], Endpoint %d, GPIO %d) changed to: %s\r\n",
    lightName[i], i, OnOffLight[i].getEndPointId(), lightPins[i], state ? "ON" : "OFF"
  );
  return true;
});
```

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. After commissioning, you will see six separate on/off light devices in your smart home app, each representing a different room.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as six separate on/off lights in your Home app (Room 1 through Room 6)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The six lights will appear in your Alexa app
6. You can control each room independently

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. The six lights will appear in your Google Home app

## Code Structure

The MatterLambdaSingleCallbackManyEPs example consists of the following main components:

1. **Arrays and Configuration**:
   - `OnOffLight[MAX_LIGHT_NUMBER]`: Array of Matter on/off light endpoints
   - `lightPins[]`: Array of GPIO pins for each light
   - `lightName[]`: Array of friendly names for each light

2. **`setup()`**: Configures Wi-Fi (if needed), initializes all GPIO pins, initializes all Matter endpoints, registers lambda callbacks with capture variables for each endpoint, and starts the Matter stack.

3. **`loop()`**: Checks the Matter commissioning state and connection status, displays appropriate messages, and allows the Matter stack to process events.

4. **Lambda Callback**:
   - Uses capture variable `[i]` to identify which endpoint triggered the callback
   - Displays detailed information including friendly name, array index, endpoint ID, GPIO pin, and state
   - Demonstrates efficient callback handling for multiple endpoints

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Only some endpoints appear**: Some smart home platforms may group or display endpoints differently. Check your app's device list
- **GPIO pins not responding**: Verify pin configurations match your hardware. Ensure pins are not used by other peripherals
- **Failed to commission**: Try erasing the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection
- **Compilation errors with lambda functions**: Ensure you're using a C++11 compatible compiler (ESP32 Arduino Core 2.0+ supports this)

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)

## License

This example is licensed under the Apache License, Version 2.0.
