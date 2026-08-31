# Matter Status Example

This example demonstrates how to check enabled Matter features and connectivity status using the Matter library's capability query functions. It implements a basic on/off light device and periodically reports the status of enabled features and network connections.

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

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `WiFi.begin(ssid, password)`.
- **ESP32-C6:** prebuild is dual-stack. Without `selectNetwork()` this sketch uses **Wi-Fi + CHIPoBLE**. Thread stays unused.
- **ESP32-H2:** Thread + CHIPoBLE (no Wi-Fi).
- **ESP32-C5:** Wi-Fi + CHIPoBLE. Thread is not in that prebuild.

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (ESP32-C6 / ESP32-H2): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (ESP32-C6 / ESP32-H2): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Matter protocol implementation for an on/off light device
- **Capability reporting**: Checks and reports enabled Matter features at startup
  - `isWiFiStationEnabled()`: Checks if Wi-Fi Station mode is supported and enabled
  - `isWiFiAccessPointEnabled()`: Checks if Wi-Fi AP mode is supported and enabled
  - `isThreadEnabled()`: Checks if Thread network is supported and enabled
  - `isBLECommissioningEnabled()`: Checks if BLE commissioning is supported and enabled
  - `isBLEMemoryReleaseEnabled()`: Checks if BLE RAM will be released after CHIPoBLE commissioning
- **Connection status monitoring**: Reports commissioned / connected / radios every 5 seconds. Samples `isOnline()` every 2.5 seconds and prints a line when any of those flags change
  - `isWiFiConnected()`: Checks Wi-Fi connection status (if Wi-Fi Station is enabled)
  - `isThreadConnected()`: Checks Thread connection status (if Thread is enabled)
  - `isDeviceConnected()`: Checks overall device connectivity (Wi-Fi, Thread, or Ethernet IPv6)
  - `isDeviceCommissioned()`: Checks if the device is commissioned to a Matter fabric
  - `isOnline()`: Checks if a controller has an active CASE session with this node (not a gate for the LED). Stays true until the session is idle-evicted, not when the user closes the app.
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
   - `WiFi` (only for ESP32 and ESP32-S2)

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
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
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
Wi-Fi Station Enabled: YES
Wi-Fi Access Point Enabled: NO
Thread Enabled: NO
BLE Commissioning Enabled: NO

Connecting to your-ssid
.......
Wi-Fi connected
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
Wi-Fi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: NO
Device Online (CASE): NO

=== Connection Status ===
Wi-Fi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: NO
Device Online (CASE): NO

... (reports every 5 seconds)

User Callback :: New Light State = ON
=== Connection Status ===
Wi-Fi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: YES
Device Online (CASE): NO

State change: Commissioned=YES Connected=YES Online=YES
=== Connection Status ===
Wi-Fi Connected: YES
Thread Connected: NO
Device Connected: YES
Device Commissioned: YES
Device Online (CASE): YES

... (reports every 5 seconds)
```

## Usage

### Capability Queries

The example demonstrates the use of capability query functions that check both hardware support (SoC capabilities) and Matter configuration:

- **`Matter.isWiFiStationEnabled()`**: Returns `true` if the device supports Wi-Fi Station mode and it's enabled in Matter configuration
- **`Matter.isWiFiAccessPointEnabled()`**: Returns `true` if the device supports Wi-Fi AP mode and it's enabled in Matter configuration
- **`Matter.isThreadEnabled()`**: Returns `true` if the device supports Thread networking and it's enabled in Matter configuration
- **`Matter.isBLECommissioningEnabled()`**: Returns `true` if the device supports BLE and BLE commissioning is enabled
- **`Matter.isBLEMemoryReleaseEnabled()`**: Returns `true` if CHIPoBLE is on and BLE RAM will be released after commissioning

These functions are useful for:
- Determining which features are available on the current device
- Adapting application behavior based on available capabilities
- Debugging configuration issues

### Connection Status Monitoring

The example reports commissioned / connected / radio status every 5 seconds and samples `isOnline()` every 2.5 seconds:

- **`Matter.isWiFiConnected()`**: Returns `true` if Wi-Fi Station is connected. If Wi-Fi Station is not enabled, always returns `false`.
- **`Matter.isThreadConnected()`**: Returns `true` if Thread is attached to a network. If Thread is not enabled, always returns `false`.
- **`Matter.isDeviceConnected()`**: Returns `true` if the device is connected via Wi-Fi, Thread, or Ethernet IPv6
- **`Matter.isDeviceCommissioned()`**: Returns `true` if the device has been commissioned to a Matter fabric
- **`Matter.isOnline()`**: Returns `true` if a Matter controller currently has an active CASE (operational) session. This is not the same as commissioned (fabric exists) or connected (radio/IP is up). It stays true until CHIP or the hub tears that session down (idle-evict), not merely when the user closes an app. The on/off LED is **not** gated on this flag.

Typical first-commission sequence: pairing code → `Commissioned=YES` → `Connected=YES` → `Online=YES`. Online changes can appear on the 2.5 s poll; commissioned / connected changes appear on the 5 s poll.

### Smart Home Integration

Use a Matter-compatible hub (like an Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device. Once commissioned, you can control the light from your smart home app.

## Code Structure

- **`setup()`**:
  - Initializes hardware (LED)
  - Reports enabled features using capability query functions
  - Connects to Wi-Fi (if needed and enabled)
  - Initializes On/Off Light endpoint
  - Starts Matter stack
  - Prints commissioning information

- **`loop()`**:
  - Samples `isOnline()` every 2.5 seconds; reports commissioned / connected / radios every 5 seconds
  - All light control is handled via Matter callbacks

- **Callbacks**:
  - `setLightOnOff()`: Controls the physical LED based on the on/off state and prints the state change to Serial Monitor

## Troubleshooting

1. **Device not discoverable**: Ensure Wi-Fi is connected (for ESP32/ESP32-S2) or BLE is enabled (for other chips).

2. **Capability queries return unexpected values**: These functions check both hardware support and Matter configuration. Verify that the features are enabled in your Matter build configuration.

3. **Connection status not updating**: Radios and commissioned / connected are reported every 5 seconds; `isOnline()` is sampled every 2.5 seconds. Check Serial Monitor output to see the periodic reports.

4. **LED not responding**: Verify pin configurations and connections.

5. **Failed to commission**: Try factory resetting the device by calling `Matter.decommission()`. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter On/Off Light Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_on_off_light.html)

## License

This example is licensed under the Apache License, Version 2.0.
