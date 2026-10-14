# Matter Soil Sensor Example

This example demonstrates how to create a Matter-compatible soil sensor device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, sensor data reporting to smart home ecosystems, and automatic simulation of soil moisture readings.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       |
| -------- | ---------------------- | -------- | ---------------------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C5 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD)`.
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

- Matter protocol implementation for a soil sensor device (device type `0x0045`, Soil Measurement cluster `0x0430`)
- Default network and CHIPoBLE as in the Supported Targets table (ESP32-C6 dual-stack uses Wi-Fi unless you call `selectNetwork()`)
- Soil moisture reporting as whole percent (0–100); no sub-percent precision
- Automatic simulation of soil moisture readings (20% to 60% range)
- Periodic sensor updates every 5 seconds
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

`begin()` creates the endpoint only. Call `setSoilMoisture()` after `Matter.begin()` when the live `SoilMeasurementCluster` is registered.

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- User button for factory reset (uses BOOT button by default)
- Optional: capacitive or resistive soil moisture probe — replace the simulation function

## Pin Configuration

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

2. **Button pin configuration** (optional):
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;
   ```

3. **Real sensor integration** (optional):
   Replace `getSimulatedSoilMoisture()` with code that returns `uint8_t` in the range 0–100.

## Building and Flashing

1. Open the sketch in the Arduino IDE (**File → Examples → Matter → Sensors → MatterSoilSensor**).
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. Wi-Fi connection messages appear only on ESP32 and ESP32-S2. CHIPoBLE targets get the operational network from the hub (Wi-Fi, or Thread on ESP32-C5 / ESP32-C6 / ESP32-H2). You should see output similar to the following:

```
Connecting to your-wifi-ssid
.......
Wi-Fi connected
IP address: 192.168.1.100

Matter Node is not commissioned yet.
Commission it using the pairing code or QR code.
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=...
[ready] net=wifi commissioned=N connected=N controller=N
...
[ready] net=wifi commissioned=Y connected=Y controller=Y
Controller CASE session is up.
Current Soil Moisture is 21%
Current Soil Moisture is 22%
...
```

## Using the Device

### Manual Control

- **Long press (>5 seconds)** on the BOOT button: factory reset (decommission)

### Sensor Simulation

The example cycles soil moisture from 20% to 60% in 1% steps every 5 seconds. Map your probe reading to 0–100 before calling `setSoilMoisture()`.

### Smart Home Integration

Commission with a Matter-compatible hub (Home Assistant, Apple Home, Google Home, Amazon Alexa) using the QR code or manual pairing code from the serial log.

## Code Structure

1. **`setup()`**: `MatterButton`, Wi-Fi if needed, `SoilSensor.begin()`, `matterSetExampleIdentity()`, `Matter.begin()`, `matterWaitUntilReady()`, then first `setSoilMoisture()`.
2. **`loop()`**: `matterRestartIfNoFabric()`, periodic print/update every 5 s, long-hold decommission via `MatterButton`.
3. **`getSimulatedSoilMoisture()`**: Replace with your sensor driver.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **Readings not updating**: Verify `setSoilMoisture()` runs after `Matter.begin()` and returns `true`
- **Values out of range**: Soil Measurement accepts whole percent 0–100 only
- **Failed to commission**: Long-press factory reset or erase flash before upload
- **No serial output**: Check baud rate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Soil Sensor Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_soil_sensor.html)

## License

This example is licensed under the Apache License, Version 2.0.
