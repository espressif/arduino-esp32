# Matter Status Example

This example demonstrates how to check enabled Matter features and connectivity status using the Matter library's capability query functions. It implements a basic on/off light device and periodically reports the status of enabled features and network connections.

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
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Matter protocol implementation for an on/off light device
- **Capability reporting**: Checks and reports enabled Matter features at startup
  - `isWiFiStationEnabled()`: Checks if WiFi Station mode is supported and enabled
  - `isWiFiAccessPointEnabled()`: Checks if WiFi AP mode is supported and enabled
  - `isThreadEnabled()`: Checks if Thread network is supported and enabled
  - `isBLECommissioningEnabled()`: Checks if BLE commissioning is supported and enabled
- **Connection status monitoring**: Reports connection status every 10 seconds
  - `isWiFiConnected()`: Checks WiFi connection status (if WiFi Station is enabled)
  - `isThreadConnected()`: Checks Thread connection status (if Thread is enabled)
  - `isDeviceConnected()`: Checks overall device connectivity (WiFi or Thread)
  - `isDeviceCommissioned()`: Checks if the device is commissioned to a Matter fabric
- Simple on/off light control
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- LED connected to GPIO pin (or using built-in LED) for visual feedback

## Pin Configuration

- **LED**: Uses `LED_BUILTIN` if defined, otherwise pin 2

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `Wi-Fi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi Credentials** (for ESP32 and ESP32-S2 only):
   ```cpp
   const char *ssid = "your-ssid";
   const char *password = "your-password";
   ```

2. **LED pin configuration** (if not using built-in LED):
   ```cpp
   const uint8_t ledPin = 2;  // Set your LED pin here
   ```

## Building and Flashing

1. Open the `MatterStatus.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
========================================
Matter Status Example
========================================

=== Enabled Features ===
WiFi Station Enabled: YES
WiFi Access Point Enabled: NO
Thread Enabled: NO
BLE Commissioning Enabled: NO

Connecting to your-ssid
.......
WiFi connected
IP address: 192.168.1.100
Matter started

========================================
Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT:Y.K9042C00KA0648G00
========================================

=== Connection Status ===
WiFi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: NO

=== Connection Status ===
WiFi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: NO

... (reports every 10 seconds)

User Callback :: New Light State = ON
=== Connection Status ===
WiFi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: YES

... (reports every 10 seconds)
```

## Usage

### Capability Queries

The example demonstrates the use of capability query functions that check both hardware support (SOC capabilities) and Matter configuration:

- **`Matter.isWiFiStationEnabled()`**: Returns `true` if the device supports WiFi Station mode and it's enabled in Matter configuration
- **`Matter.isWiFiAccessPointEnabled()`**: Returns `true` if the device supports WiFi AP mode and it's enabled in Matter configuration
- **`Matter.isThreadEnabled()`**: Returns `true` if the device supports Thread networking and it's enabled in Matter configuration
- **`Matter.isBLECommissioningEnabled()`**: Returns `true` if the device supports BLE and BLE commissioning is enabled

These functions are useful for:
- Determining which features are available on the current device
- Adapting application behavior based on available capabilities
- Debugging configuration issues

### Connection Status Monitoring

The example periodically reports connection status every 10 seconds:

- **`Matter.isWiFiConnected()`**: Returns `true` if WiFi Station is connected (only available if WiFi Station is enabled)
- **`Matter.isThreadConnected()`**: Returns `true` if Thread is attached to a network (only available if Thread is enabled)
- **`Matter.isDeviceConnected()`**: Returns `true` if the device is connected via WiFi or Thread (overall connectivity status)
- **`Matter.isDeviceCommissioned()`**: Returns `true` if the device has been commissioned to a Matter fabric

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. Once commissioned, you can control the light from your smart home app.

## Code Structure

- **`setup()`**: 
  - Initializes hardware (LED)
  - Reports enabled features using capability query functions
  - Connects to WiFi (if needed and enabled)
  - Initializes On/Off Light endpoint
  - Starts Matter stack
  - Prints commissioning information

- **`loop()`**: 
  - Reports connection status every 10 seconds
  - All light control is handled via Matter callbacks

- **Callbacks**:
  - `setLightOnOff()`: Controls the physical LED based on the on/off state and prints the state change to Serial Monitor

## Troubleshooting

1. **Device not discoverable**: Ensure Wi-Fi is connected (for ESP32/ESP32-S2) or BLE is enabled (for other chips).

2. **Capability queries return unexpected values**: These functions check both hardware support and Matter configuration. Verify that the features are enabled in your Matter build configuration.

3. **Connection status not updating**: The status is reported every 10 seconds. Check Serial Monitor output to see the periodic reports.

4. **LED not responding**: Verify pin configurations and connections.

5. **Failed to commission**: Try factory resetting the device by calling `Matter.decommission()`. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter On/Off Light Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_on_off_light.html)

## License

This example is licensed under the Apache License, Version 2.0.

