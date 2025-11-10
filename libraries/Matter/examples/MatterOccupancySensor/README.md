# Matter Occupancy Sensor Example

This example demonstrates how to create a Matter-compatible occupancy sensor device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, sensor data reporting to smart home ecosystems, and automatic simulation of occupancy state changes.

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

- Matter protocol implementation for an occupancy sensor device
- Support for both Wi-Fi and Thread(*) connectivity
- Occupancy state reporting (Occupied/Unoccupied)
- Automatic simulation of occupancy state changes every 2 minutes
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Apple HomeKit, Amazon Alexa, and Google Home
(*) It is necessary to compile the project using Arduino as IDF Component.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for factory reset (uses BOOT button by default)
- Optional: PIR (Passive Infrared) motion sensor (e.g., HC-SR501, AM312) for real occupancy detection

## Pin Configuration

- **Button**: Uses `BOOT_PIN` by default
- **PIR Sensor** (optional): Connect to any available GPIO pin (e.g., GPIO 4) when using a real sensor

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

3. **PIR sensor pin configuration** (optional, if using a real PIR sensor):
   ```cpp
   const uint8_t pirPin = 4;  // Set your PIR sensor pin here
   ```
   See the "PIR Sensor Integration Example" section below for complete integration instructions.

## Building and Flashing

1. Open the `MatterOccupancySensor.ino` sketch in the Arduino IDE.
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
```

After commissioning, the occupancy sensor will automatically toggle between occupied and unoccupied states every 2 minutes, and the Matter controller will receive these state updates.

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Sensor Simulation

The example includes a simulated occupancy sensor that:

- Starts in the unoccupied state (false)
- Toggles between occupied (true) and unoccupied (false) every 2 minutes
- Updates the Matter attribute automatically

To use a real occupancy sensor, replace the `simulatedHWOccupancySensor()` function with your sensor library code.

### PIR Sensor Integration Example

Here's a complete example for integrating a simple PIR (Passive Infrared) motion sensor:

#### Hardware Connections

Connect the PIR sensor to your ESP32:
- **PIR VCC** → ESP32 3.3 V or 5 V (check your PIR sensor specifications)
- **PIR GND** → ESP32 GND
- **PIR OUT** → ESP32 GPIO pin (e.g., GPIO 4)

Common PIR sensors (HC-SR501, AM312, etc.) typically have three pins: VCC, GND, and OUT (digital output).

#### Code Modifications

1. **Add PIR pin definition** at the top of the sketch (after the button pin definition):

```cpp
// PIR sensor pin
const uint8_t pirPin = 4;  // Change this to your PIR sensor pin
```

2. **Initialize PIR pin in setup()** (after button initialization):

```cpp
void setup() {
  // ... existing code ...
  pinMode(buttonPin, INPUT_PULLUP);

  // Initialize PIR sensor pin
  pinMode(pirPin, INPUT);

  // ... rest of setup code ...
}
```

3. **Replace the simulated function** with the real PIR reading function:

```cpp
bool simulatedHWOccupancySensor() {
  // Read PIR sensor digital input
  // HIGH = motion detected (occupied), LOW = no motion (unoccupied)
  return digitalRead(pirPin) == HIGH;
}
```

#### Complete Modified Function Example

Here's the complete modified function with debouncing for more reliable readings:

```cpp
bool simulatedHWOccupancySensor() {
  // Read PIR sensor with debouncing
  static bool lastState = false;
  static uint32_t lastChangeTime = 0;
  const uint32_t debounceTime = 100;  // 100ms debounce

  bool currentState = digitalRead(pirPin) == HIGH;

  // Only update if state has changed and debounce time has passed
  if (currentState != lastState) {
    if (millis() - lastChangeTime > debounceTime) {
      lastState = currentState;
      lastChangeTime = millis();
      Serial.printf("Occupancy state changed: %s\r\n", currentState ? "OCCUPIED" : "UNOCCUPIED");
    }
  }

  return lastState;
}
```

#### Testing the PIR Sensor

After making these changes:
1. Upload the modified sketch to your ESP32
2. Open the Serial Monitor at 115200 baud
3. Move in front of the PIR sensor - you should see "OCCUPIED" messages
4. Stay still for a few seconds - you should see "UNOCCUPIED" messages
5. The Matter controller will automatically receive these occupancy state updates

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an occupancy sensor in your Home app
7. You can monitor the occupancy state and set up automations based on occupancy (e.g., turn on lights when occupied)

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The occupancy sensor will appear in your Alexa app
6. You can monitor occupancy readings and create routines based on occupancy state changes

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. You can monitor occupancy readings and create automations based on occupancy state changes

## Code Structure

The MatterOccupancySensor example consists of the following main components:

1. **`setup()`**: Initializes hardware (button), configures Wi-Fi (if needed), sets up the Matter Occupancy Sensor endpoint with initial state (unoccupied), and waits for Matter commissioning.

2. **`loop()`**: Handles button input for factory reset, continuously checks the simulated occupancy sensor and updates the Matter attribute, and allows the Matter stack to process events.

3. **`simulatedHWOccupancySensor()`**: Simulates a hardware occupancy sensor by toggling the occupancy state every 2 minutes. Replace this function with your actual sensor reading code.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Occupancy readings not updating**: Check that the sensor simulation function is being called correctly. For real sensors, verify sensor wiring and library initialization
- **State not changing**: The simulated sensor toggles every 2 minutes (120000 ms). If you're using a real sensor, ensure it's properly connected and reading correctly
- **PIR sensor not detecting motion**:
  - Verify PIR sensor wiring (VCC, GND, OUT connections)
  - Check if PIR sensor requires 5 V or 3.3 V power (some PIR sensors need 5 V)
  - Allow 30-60 seconds for PIR sensor to stabilize after power-on
  - Adjust PIR sensor sensitivity and time delay potentiometers (if available on your sensor)
  - Ensure the PIR sensor has a clear view of the detection area
  - Test the PIR sensor directly by reading the GPIO pin value in Serial Monitor
- **PIR sensor false triggers**: Add debouncing to the sensor reading function (see the "Complete Modified Function Example" above)
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Occupancy Sensor Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_occupancy_sensor.html)

## License

This example is licensed under the Apache License, Version 2.0.
