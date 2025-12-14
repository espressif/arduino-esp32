# Matter Simple Blinds Example

This is a minimal example demonstrating how to create a Matter-compatible window covering device with lift control only. This example uses a single `onGoToLiftPercentage()` callback to handle all window covering lift changes, making it ideal for simple implementations.

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
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project as an ESP-IDF component and to disable the Matter Wi-Fi station feature.

## Features

- Matter protocol implementation for a window covering device
- **Lift control only** (0-100%) - simplified implementation
- **Single `onGoToLiftPercentage()` callback** - handles all window covering lift changes when `TargetPositionLiftPercent100ths` changes
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Window covering motor/actuator (optional for testing - example simulates movement)

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

## Building and Flashing

1. Open the `MatterSimpleWindowBlind.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

```
========================================
Matter Simple Blinds Example
========================================

Connecting to your-ssid
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
```

When a command is received from the Matter controller:
```
Window Covering change request: Lift=50%
```

## Usage

1. **Commissioning**: Use the QR code or manual pairing code to commission the device to your Matter hub (Apple Home, Google Home, or Amazon Alexa).

2. **Control**: Once commissioned, you can control the window covering lift percentage (0-100%) from your smart home app. The `onGoToLiftPercentage()` callback will be triggered whenever the target lift percentage changes.

## Code Structure

- **`onBlindLift()`**: Callback function that handles window covering lift changes. This is registered with `WindowBlinds.onGoToLiftPercentage()` and is triggered when `TargetPositionLiftPercent100ths` changes. The callback receives the target lift percentage (0-100%).
- **`setup()`**: Initializes Wi-Fi (if needed), Window Covering endpoint with `ROLLERSHADE` type, registers the callback, and starts Matter.
- **`loop()`**: Empty - all control is handled via Matter callbacks.

## Customization

### Adding Motor Control

In the `onBlindLift()` callback, replace the simulation code with actual motor control:

```cpp
bool onBlindLift(uint8_t liftPercent) {
  Serial.printf("Moving window covering to %d%%\r\n", liftPercent);

  // Here you would control your actual motor/actuator
  // For example:
  // - Calculate target position based on liftPercent and installed limits (if configured)
  // - Move motor to target position
  // - When movement is complete, update current position:
  //   WindowBlinds.setLiftPercentage(finalLiftPercent);
  //   WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);

  // For this minimal example, we just return true to accept the command
  return true;  // Indicate command was accepted
}
```

## Troubleshooting

1. **Device not discoverable**: Ensure Wi-Fi is connected (for ESP32/ESP32-S2) or BLE is enabled (for other chips).

2. **Lift percentage not updating**: Check that `onGoToLiftPercentage()` callback is properly registered and that `setLiftPercentage()` is called when movement is complete to update the `CurrentPosition` attribute.

3. **Commands not working**: Ensure the callback returns `true` to accept the command. If it returns `false`, the command will be rejected.

4. **Motor not responding**: Replace the simulation code in `onBlindLift()` with your actual motor control implementation. Remember to update `CurrentPosition` and set `OperationalState` to `STALL` when movement is complete.

## Notes

- This example uses `ROLLERSHADE` window covering type (lift only, no tilt).
- The example accepts commands but doesn't actually move a motor. In a real implementation, you should:
  1. Move the motor to the target position in the callback
  2. Update `CurrentPositionLiftPercent100ths` using `setLiftPercentage()` when movement is complete
  3. Set `OperationalState` to `STALL` using `setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL)` to indicate the device has reached the target position
- **Important**: `onGoToLiftPercentage()` is called when `TargetPositionLiftPercent100ths` changes. This happens when commands are executed or when a Matter controller writes directly to the target position attribute.
- Commands modify `TargetPosition`, not `CurrentPosition`. The application is responsible for updating `CurrentPosition` when the physical device actually moves.
