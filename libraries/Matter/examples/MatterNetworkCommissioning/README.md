# Matter Network Commissioning Example

This example demonstrates how to control which networks (WiFi/Thread) are available for commissioning when using CHIPoBLE (Chip over Bluetooth Low Energy) with an ESP32 SoC microcontroller.\
The application showcases Matter network commissioning control, enabling/disabling WiFi and Thread commissioning modes, and monitoring commissioning events via serial commands.

## Supported Targets

| SoC | WiFi | Thread | BLE Commissioning | Status |
| --- | ---- | ------ | ----------------- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Limited (WiFi only, no BLE) |
| ESP32-S2 | ✅ | ❌ | ❌ | Limited (WiFi only, no BLE) |
| ESP32-S3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C6 | ✅ | ✅ | ✅ | Fully supported |
| ESP32-C5 | ✅ | ✅ | ✅ | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Fully supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, network commissioning control is limited. This example is primarily designed for chips that support BLE commissioning (ESP32-S3, ESP32-C3, ESP32-C6, ESP32-C5, ESP32-H2).
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using WiFi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter WiFi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been pre compiled using WiFi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter WiFi station feature.

## Features

- Matter protocol implementation with network commissioning control
- Enable/disable WiFi network commissioning dynamically
- Enable/disable Thread network commissioning dynamically
- Set specific network commissioning modes (WiFi-only, Thread-only, both, or none)
- Serial command interface for controlling network commissioning
- Real-time network status monitoring
- Matter event callback for commissioning events
- Support for both WiFi and Thread(*) connectivity
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- USB connection for serial communication

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `WiFi`

## Building and Flashing

1. Open the `MatterNetworkCommissioning.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
3. Connect your ESP32 board to your computer via USB.
4. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. You should see output similar to the following:

```
Matter Network Commissioning Control Example
===========================================

--- Network Commissioning Status ---
WiFi Commissioning: ENABLED
Thread Commissioning: DISABLED
Device Commissioned: NO
Device Connected: NO
----------------------------------

--- Available Commands ---
w/W : Toggle WiFi commissioning
t/T : Toggle Thread commissioning
r/R : Reset to factory defaults
s/S : Show network status
1   : WiFi-only mode
2   : Thread-only mode
3   : Both networks mode
0   : No networks mode
h/H/? : Show this help
-------------------------
```

## Using the Device

### Serial Commands

The example provides a serial command interface to control network commissioning modes. Type the following commands in the Serial Monitor:

#### Individual Network Control

- **`w` or `W`**: Toggle WiFi commissioning on/off
- **`t` or `T`**: Toggle Thread commissioning on/off

#### Predefined Modes

- **`1`**: Set WiFi-only commissioning mode
- **`2`**: Set Thread-only commissioning mode
- **`3`**: Set both networks commissioning mode (WiFi and Thread)
- **`0`**: Disable all network commissioning

#### Utility Commands

- **`s` or `S`**: Show current network status
- **`r` or `R`**: Reset to factory defaults (decommission and restart)
- **`h`, `H`, or `?`**: Show help (available commands)

### Example Usage

1. **Start with WiFi-only mode**:
   ```
   Type '1' in Serial Monitor
   Output: ✓ WiFi-only mode set successfully
   ```

2. **Enable Thread commissioning**:
   ```
   Type 't' in Serial Monitor
   Output: ✓ Thread commissioning enabled
   ```

3. **Check current status**:
   ```
   Type 's' in Serial Monitor
   Output: Shows current network commissioning status
   ```

4. **Set both networks mode**:
   ```
   Type '3' in Serial Monitor
   Output: ✓ Both networks mode set successfully
   ```

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. The network commissioning mode determines which networks are available during the commissioning process.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an on/off light in your Home app

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The light will appear in your Alexa app

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup

## Code Structure

The MatterNetworkCommissioning example consists of the following main components:

1. **`setup()`**: Initializes Serial communication, registers Matter event callback, initializes the Matter on/off light endpoint, sets initial network commissioning mode (WiFi-only by default), starts the Matter stack, synchronizes network states, and displays initial status.

2. **`loop()`**: Handles serial commands from the user and allows the Matter stack to process events.

3. **Serial Command Handlers**:
   - `handleSerialCommand()`: Routes commands to appropriate handlers
   - `toggleWiFiCommissioning()`: Enables/disables WiFi commissioning
   - `toggleThreadCommissioning()`: Enables/disables Thread commissioning
   - `setWiFiOnlyMode()`: Sets WiFi-only commissioning mode
   - `setThreadOnlyMode()`: Sets Thread-only commissioning mode
   - `setBothNetworksMode()`: Enables both WiFi and Thread commissioning
   - `setNoNetworksMode()`: Disables all network commissioning
   - `resetToFactory()`: Performs factory reset (decommission and restart)

4. **Utility Functions**:
   - `syncNetworkStates()`: Synchronizes local state variables with actual API state
   - `printNetworkStatus()`: Displays current network commissioning status
   - `printCommands()`: Displays available serial commands

5. **Event Callback**:
   - `onMatterEvent()`: Handles Matter events such as commissioning completion, session start/stop, WiFi/Thread connectivity changes, and CHIPoBLE connection events

## Network Commissioning Modes

The example supports the following network commissioning modes:

- **WiFi-only**: Only WiFi networks are available for commissioning
- **Thread-only**: Only Thread networks are available for commissioning
- **Both networks**: Both WiFi and Thread networks are available for commissioning
- **No networks**: All network commissioning is disabled (device cannot be commissioned via networks)

## Troubleshooting

- **Device not visible during commissioning**: Ensure network commissioning is enabled for the desired network type (WiFi or Thread). Check the network status using the 's' command
- **WiFi commissioning not supported**: This message appears if WiFi station support is not compiled into the Matter library for your board
- **Thread commissioning not supported**: This message appears if Thread support is not compiled into the Matter library for your board
- **Commands not working**: Ensure you're typing commands in the Serial Monitor (not Serial Plotter) and that the baud rate is set to 115200
- **State mismatch warnings**: If you see state mismatch warnings, the API has been synchronized to match the actual state. This is normal and handled automatically
- **Failed to commission**: Try changing the network commissioning mode (e.g., switch from Thread-only to WiFi-only) and attempt commissioning again. You can also use the 'r' command to factory reset
- **No serial output**: Check baudrate (115200) and USB connection

## License

This example is licensed under the Apache License, Version 2.0.

