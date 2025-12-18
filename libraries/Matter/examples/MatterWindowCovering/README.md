# Matter Window Covering Example

This example demonstrates how to create a Matter-compatible window covering device using an ESP32 SoC microcontroller. The application showcases Matter commissioning, device control via smart home ecosystems, and manual control using a physical button.

## Supported Targets

| SoC | Wi-Fi | Thread | BLE Commissioning | Status |
| --- | ---- | ------ | ----------------- | ------ |
| ESP32 | ✅ | ❌ | ❌ | Fully supported |
| ESP32-S2 | ✅ | ❌ | ❌ | Fully supported |
| ESP32-S3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C3 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-C5 | ❌ | ✅ | ✅ | Supported (Thread only) |
| ESP32-C6 | ✅ | ❌ | ✅ | Fully supported |
| ESP32-H2 | ❌ | ✅ | ✅ | Supported (Thread only) |

### Note on Commissioning:

- **ESP32 & ESP32-S2** do not support commissioning over Bluetooth LE. For these chips, you must provide Wi-Fi credentials directly in the sketch code so they can connect to your network manually.
- **ESP32-C6** Although it has Thread support, the ESP32 Arduino Matter Library has been precompiled using Wi-Fi only. In order to configure it for Thread-only operation it is necessary to build the project using Arduino as an IDF Component and to disable the Matter Wi-Fi station feature.
- **ESP32-C5** Although it has Wi-Fi 2.4 GHz and 5 GHz support, the ESP32 Arduino Matter Library has been pre compiled using Thread only. In order to configure it for Wi-Fi operation it is necessary to build the project using Arduino as an ESP-IDF component and disable Thread network, keeping only Wi-Fi station.

## Features

- Matter protocol implementation for a window covering device
- Support for both Wi-Fi and Thread(*) connectivity
- Lift position and percentage control (0-100%) - Lift represents the physical position (centimeters)
- Tilt rotation and percentage control (0-100%) - Tilt represents rotation of the shade, not a linear measurement
- Multiple window covering types support
- State persistence using `Preferences` library
- Button control for manual lift adjustment and factory reset
- RGB LED visualization of lift (brightness) and tilt (color) positions
- Installed limit configuration for lift (cm) and tilt (absolute values)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- RGB LED for visualization (uses built-in RGB LED if available)
- User button for manual control (uses BOOT button by default)

## Pin Configuration

- **RGB LED**: Uses `RGB_BUILTIN` if defined (for visualization), otherwise pin 2. The LED brightness represents lift position (0% = off, 100% = full brightness), and color represents tilt rotation (red = 0%, blue = 100%).
- **Button**: Uses `BOOT_PIN` by default

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

2. **RGB LED pin configuration** (if not using built-in RGB LED):
   ```cpp
   const uint8_t ledPin = 2;  // Set your RGB LED pin here
   ```

3. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for manual lift control and factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterWindowCovering.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. The Wi-Fi connection messages will be displayed only for ESP32 and ESP32-S2. Other targets will use Matter CHIPoBLE to automatically setup the IP Network. You should see output similar to the following:

```
Connecting to your-wifi-ssid
.......
WiFi connected
IP address: 192.168.1.100

Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A6FCJ142C00KA0648G00
Matter Node not commissioned yet. Waiting for commissioning.
...
Initial state: Lift=100%, Tilt=0%
Matter Node is commissioned and connected to the network. Ready for use.
Window Covering changed: Lift=100%, Tilt=0%
Moving lift to 50% (position: 100 cm)
Window Covering changed: Lift=50%, Tilt=0%
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Cycle lift percentage by 20% increments. If the current position is not a multiple of 20%, it will round up to the next multiple of 20%. Otherwise, it will add 20% (0% → 20% → 40% → ... → 100% → 0%)
- **Long press (>5 seconds)**: Factory reset the device (decommission)

### State Persistence

The device saves the last known lift and tilt percentages using the `Preferences` library. After a power cycle or restart:

- The device will restore to the last saved lift and tilt percentages
- Default state is 100% lift (fully open) and 0% tilt if no previous state was saved
- The Matter controller will be notified of the restored state
- The RGB LED will reflect the restored state

### RGB LED Visualization

The RGB LED provides visual feedback:

- **Brightness**: Represents lift position (0% = off, 100% = full brightness)
- **Color**: Represents tilt position (red = 0% tilt, blue = 100% tilt)
- For boards without RGB LED, only brightness is used

### Window Covering Integration

For production use with a motorized window covering:

1. **Motor Control**:
   - Connect your motor driver to your ESP32
   - Update the callback functions (`fullOpen()`, `fullClose()`, `goToLiftPercentage()`, `goToTiltPercentage()`, `stopMotor()`) to control your actual motor
   - The example currently simulates movement instantly - replace with actual motor control code

2. **Position Feedback**:
   - Use encoders or limit switches to provide position feedback
   - For lift: Update `currentLift` (cm) based on actual motor position
   - For tilt: Update `currentTiltPercent` (rotation percentage) based on actual motor rotation
   - **Important**: Call `setLiftPercentage()` and `setTiltPercentage()` in your `onGoToLiftPercentage()` or `onGoToTiltPercentage()` callbacks to update `CurrentPosition` attributes when the physical device actually moves. This will trigger the `onChange()` callback if registered.
   - Call `setOperationalState(LIFT, STALL)` or `setOperationalState(TILT, STALL)` when movement is complete to indicate the device has reached the target position
   - Configure installed limits using `setInstalledOpenLimitLift()`, `setInstalledClosedLimitLift()`, `setInstalledOpenLimitTilt()`, and `setInstalledClosedLimitTilt()` to define the physical range of your window covering

   **Callback Flow:**
   - Matter command → `TargetPosition` changes → `onGoToLiftPercentage()`/`onGoToTiltPercentage()` called
   - Your callback moves the motor → When movement completes, call `setLiftPercentage()`/`setTiltPercentage()`
   - `setLiftPercentage()`/`setTiltPercentage()` update `CurrentPosition` → `onChange()` called (if registered)

3. **Window Covering Type**:
   - Pass the covering type to `begin()` to configure the appropriate type (e.g., `BLIND_LIFT_AND_TILT`, `ROLLERSHADE`, etc.)
   - Different types support different features (lift only, tilt only, or both)
   - The covering type must be specified during initialization to ensure the correct features are enabled

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as a window covering/blind in your Home app
7. You can control both lift and tilt positions using sliders

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The window covering will appear in your Alexa app
6. You can control positions using voice commands like "Alexa, set blinds to 50 percent"

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can control positions using voice commands or sliders in the app

## Code Structure

The MatterWindowCovering example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, RGB LED), configures Wi-Fi (if needed), initializes `Preferences` library, sets up the Matter window covering endpoint with the last saved state, registers callback functions, and starts the Matter stack.

2. **`loop()`**: Checks the Matter commissioning state, handles button input for manual lift control and factory reset, and allows the Matter stack to process events.

3. **Callbacks**:

   **Target Position Callbacks** (called when `TargetPosition` attributes change):
   - `fullOpen()`: Registered with `onOpen()` - called when `UpOrOpen` command is received. Moves window covering to fully open (100% lift), calls `setLiftPercentage()` to update `CurrentPosition`, and sets operational state to `STALL`
   - `fullClose()`: Registered with `onClose()` - called when `DownOrClose` command is received. Moves window covering to fully closed (0% lift), calls `setLiftPercentage()` to update `CurrentPosition`, and sets operational state to `STALL`
   - `goToLiftPercentage()`: Registered with `onGoToLiftPercentage()` - called when `TargetPositionLiftPercent100ths` changes (from commands, `setTargetLiftPercent100ths()`, or direct attribute writes). Calculates absolute position (cm) based on installed limits, calls `setLiftPercentage()` to update `CurrentPosition`, and sets operational state to `STALL` when movement is complete
   - `goToTiltPercentage()`: Registered with `onGoToTiltPercentage()` - called when `TargetPositionTiltPercent100ths` changes. Calls `setTiltPercentage()` to update `CurrentPosition`, and sets operational state to `STALL` when movement is complete
   - `stopMotor()`: Registered with `onStop()` - called when `StopMotion` command is received. Stops any ongoing movement, calls `setLiftPercentage()` and `setTiltPercentage()` to update `CurrentPosition` for both, and sets operational state to `STALL` for both

   **Current Position Callback** (called when `CurrentPosition` attributes change):
   - `onChange()`: Registered with `onChange()` - called when `CurrentPositionLiftPercent100ths` or `CurrentPositionTiltPercent100ths` change (after `setLiftPercentage()` or `setTiltPercentage()` are called). Updates RGB LED visualization to reflect current positions

   **Note:** The Target Position callbacks (`fullOpen()`, `fullClose()`, `goToLiftPercentage()`, `goToTiltPercentage()`, `stopMotor()`) call `setLiftPercentage()` or `setTiltPercentage()` to update the `CurrentPosition` attributes. This triggers the `onChange()` callback, which updates the visualization.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Window covering not responding**: Verify callback functions are properly implemented and motor control is working
- **Position not updating**: Check that `setLiftPercentage()` and `setTiltPercentage()` are being called with correct values
- **State not persisting**: Check that the `Preferences` library is properly initialized and that flash memory is not corrupted
- **RGB LED not working**: For RGB LED, verify the pin supports RGB LED control. For non-RGB boards, ensure the pin supports PWM (analogWrite)
- **Tilt not working**: Ensure the covering type supports tilt (e.g., `BLIND_LIFT_AND_TILT`, `SHUTTER`, or `BLIND_TILT_ONLY`) and that it is specified in `begin()`
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Window Covering Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_window_covering.html)

## License

This example is licensed under the Apache License, Version 2.0.
