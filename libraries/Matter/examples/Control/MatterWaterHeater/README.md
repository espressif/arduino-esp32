# Matter Water Heater Example

This example demonstrates how to create a Matter-compatible water heater device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, water heater attributes (temperature, setpoint, tank state, heat demand), and a simulated heating cycle.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       |
| -------- | ---------------------- | -------- | ---------------------- |
| ESP32    | No network in sketch   | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | No network in sketch   | Off      | Ethernet (SPI)         |
| ESP32-S3 | CHIPoBLE (hub Wi-Fi)   | On       | Ethernet (SPI)         |
| ESP32-C3 | CHIPoBLE (hub Wi-Fi)   | On       | Ethernet (SPI)         |
| ESP32-C5 | CHIPoBLE (hub)         | On       | Ethernet (SPI)         |
| ESP32-C6 | CHIPoBLE (hub, default)| On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Wi-Fi/Ethernet itself.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. Add `matterConnectWiFi()` (or another on-network path) before `Matter.begin()`, or use a commissioning example.
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

- Matter protocol implementation for a water heater device (device type 0x050F)
- Default network and CHIPoBLE as in the Supported Targets table
- Simulated tank temperature, heating setpoint, tank percentage, and heat demand
- `MatterWaterHeater::begin()` provisions Water Heater Management **EnergyManagement** and **TankPercent** features (call before `Matter.begin()`)
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- No additional hardware required; temperature and tank state are simulated in the sketch

For a production device, replace the simulation with real sensors and actuators with appropriate safety interlocks.

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`

### Configuration

Before uploading the sketch on **ESP32 / ESP32-S2**, add Wi-Fi (or copy the network setup from [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)) before `Matter.begin()`.

## Building and Flashing

1. Open the example in the Arduino IDE: **File → Examples → Matter → Control → MatterWaterHeater**.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. Wi-Fi connection messages appear only if you added Wi-Fi setup for ESP32 / ESP32-S2. CHIPoBLE targets get the operational network from the hub (Wi-Fi, or Thread on ESP32-C5 / ESP32-C6 / ESP32-H2). You should see output similar to the following:

```
Matter Water Heater
-------------------
Matter Water Heater endpoint created.

Device is not commissioned.
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=...
Temperature: 20.0 C | Setpoint: 48.0 C | Tank: 0 % | Demand: 0x01 | Boost: 0
Temperature: 20.5 C | Setpoint: 48.0 C | Tank: 1 % | Demand: 0x01 | Boost: 0
...
```

## Using the Device

### Device type and clusters

The endpoint implements Matter device type **0x050F** (Water Heater) with **Water Heater Management**, **Water Heater Mode**, and **Thermostat** (heating-only) clusters.

### Simulated water heater

Default configuration:

| Parameter           |             Value |
| ------------------- | ----------------: |
| Tank volume         |             100 L |
| Initial temperature |             20 °C |
| Heating setpoint    |             48 °C |
| Heater type         | Immersion element |
| System mode         |              Heat |
| Water heater mode   |            Manual |
| Tank percentage     |               0 % |

Every five seconds the sketch increases the simulated water temperature until the heating setpoint is reached. Tank percentage is derived from temperature. **HeatDemand** follows heater activity in `loop()`; `setSystemMode()` / controller writes also sync demand via the library.

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
3. Scan the QR code displayed in the Serial Monitor, or enter the manual pairing code
4. Follow the prompts to complete setup

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup

## Code Structure

The MatterWaterHeater example consists of the following main components:

1. **`setup()`**: Creates the `MatterWaterHeater` endpoint (`begin()` adds WHM tank features), configures heater type, tank volume, setpoints, and modes, then calls `Matter.begin()`.
2. **`loop()`**: Simulates heating every five seconds, updates **HeatDemand** while below setpoint, and calls `updateTankPercentage()` from simulated temperature.
3. **`updateTankPercentage()`**: Maps temperature between cold water and setpoint to **TankPercentage**.

For a real appliance, read tank temperature from a sensor, drive the heating element with proper safety interlocks, and never rely on Matter as the only safety layer.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured (ESP32 / ESP32-S2 need Wi-Fi in the sketch or another commissioning path).
- **Tank attributes missing on hub**: Call `waterHeater.begin()` before `Matter.begin()` so EnergyManagement and TankPercent features are provisioned.
- **HeatDemand does not match hub after mode change**: Use a build that includes controller-side `syncHeatDemand()` in `attributeChangeCB`.
- **Failed to commission**: Erase flash (**Erase All Flash Before Sketch Upload**) or use another Matter node on the same fabric.
- **No serial output**: Check baud rate (115200) and USB connection.

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Water Heater Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_water_heater.html)

## License

This example is licensed under the Apache License, Version 2.0.
