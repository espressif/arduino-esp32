# Matter Water Valve Example

This example demonstrates how to create a Matter-compatible water valve device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and manual control using a physical button - suitable for irrigation valves, main water shutoff valves, or appliance water supply valves.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       | LED      |
| -------- | ---------------------- | -------- | ---------------------- | -------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) | Required |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         | Required |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C5 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI) | Required |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         | Required |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch connects to Wi-Fi with credentials in the source file.
- **ESP32-C6:** prebuild is dual-stack. Without `selectNetwork()` this sketch uses **Wi-Fi + CHIPoBLE**. Thread stays unused.
- **ESP32-H2:** Thread + CHIPoBLE (no Wi-Fi).
- **ESP32-C5:** Wi-Fi + CHIPoBLE by default (Tools → Matter Network → Wi-Fi). Thread is Tools → Matter Network → Thread.

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (ESP32-C5 / ESP32-C6 / ESP32-H2): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (ESP32-C5 / ESP32-C6 / ESP32-H2): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Matter protocol implementation for a water valve device (device type 0x0042, Valve Configuration and Control cluster)
- Default network and CHIPoBLE as in the Supported Targets table (ESP32-C6 dual-stack uses Wi-Fi unless you call `selectNetwork()`)
- Open (indefinitely or for a set duration, with automatic closing) and close control
- Automatic countdown of the `RemainingDuration` attribute for timed open operations (handled internally; no polling loop in the sketch)
- Valve fault reporting
- Button control for manual open/close and factory reset
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

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
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASSWORD "your-password"
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

1. Open the `MatterWaterValve.ino` sketch in the Arduino IDE (**File → Examples → Matter → Control → MatterWaterValve**).
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. Wi-Fi connection messages appear only on ESP32 and ESP32-S2. CHIPoBLE targets get the operational network from the hub (Wi-Fi, or Thread on ESP32-C5 / ESP32-C6 / ESP32-H2). You should see output similar to the following, which provides the necessary information for commissioning:

```
Connecting to your-wifi-ssid
.......
Wi-Fi connected
IP address: 192.168.1.100

Matter Node is not commissioned yet.
Commission it using the pairing code or QR code.
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A6FCJ142C00KA0648G00
[ready] net=wifi commissioned=N connected=N controller=N
[ready] net=wifi commissioned=Y connected=Y controller=N
...
[ready] net=wifi commissioned=Y connected=Y controller=Y
Controller CASE session is up.
User button released. Opening the water valve!
User Callback :: Opening the water valve
Water valve remaining duration: 10 s
Water valve remaining duration: 9 s
...
User Callback :: Closing the water valve
```

## Using the Device

### Manual Control

The user button (BOOT button by default) provides manual control:

- **Short press of the button**: Toggle the valve open (for `openDurationSeconds`) or closed
- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Smart Home Integration

Use a Matter-compatible hub (like a Home Assistant server, Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Home Assistant

1. Open Home Assistant
2. Go to Settings > Devices & services > Add integration > Matter
3. Scan the QR code from the Serial Monitor, or enter the manual pairing code
4. Follow the prompts to complete setup

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

1. **`setup()`**: Initializes `MatterButton` and the LED, configures Wi-Fi (if needed), sets up the Matter Water Valve endpoint, registers the `onOpen()`/`onClose()` callbacks, then `Matter.begin()` and `matterWaitUntilReady()`.

2. **`loop()`**: `matterRestartIfNoFabric()`, then `MatterButton` click (toggle open for `openDurationSeconds` or close) and long-hold decommission. Logs **RemainingDuration** while a timed open counts down. Commissioning wait is `matterWaitUntilReady()` in `setup()`, same as the Fan and Thermostat examples.

3. **Callbacks**:
   - `onValveOpen()`: Called whenever the valve is commanded open, either by a Matter controller or locally via `open()`. Drives the physical actuator (the LED, in this example). Return `true` on success or `false` if open could not be completed. A failed open is rolled back after CHIP `OpenValve()` returns so the remaining-duration timer does not keep the valve Open.
   - `onValveClose()`: Called whenever the valve is commanded closed, either by a Matter controller, locally via `close()`, or automatically when a timed open operation elapses. Return type is `void` (the Matter delegate does not use a close failure result).

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
- [Matter Water Valve Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_water_valve.html)

## License

This example is licensed under the Apache License, Version 2.0.
