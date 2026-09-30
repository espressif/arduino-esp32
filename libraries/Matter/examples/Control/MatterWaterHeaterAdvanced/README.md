# Matter Water Heater Advanced Example

This example shows every Arduino-facing Water Heater feature: mode, Eco, Boost / CancelBoost, tank percentage, and HeatDemand.\
For a shorter sketch (temperature, setpoint, Mode Off, Boost as on/off heat, HeatDemand), start with [MatterWaterHeater](../MatterWaterHeater).

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       |
| -------- | ---------------------- | -------- | ---------------------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         |
| ESP32-S3 | CHIPoBLE (hub Wi-Fi)   | On       | Ethernet (SPI)         |
| ESP32-C3 | CHIPoBLE (hub Wi-Fi)   | On       | Ethernet (SPI)         |
| ESP32-C5 | CHIPoBLE (hub)         | On       | Ethernet (SPI)         |
| ESP32-C6 | CHIPoBLE (hub, default)| On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Wi-Fi/Ethernet itself.

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

- Matter Water Heater (device type 0x050F)
- Simulated tank temperature, heating setpoint, tank percentage, and heat demand
- Water Heater Mode (Off / Manual / Eco) and Thermostat System Mode (Off / Heat)
- Hub **Boost** / **CancelBoost** (duration, one-shot, optional temporary setpoint); the sketch reads `getBoostState()` and reports temperature
- `MatterWaterHeater::begin()` provisions Water Heater Management **EnergyManagement** and **TankPercent** (call before `Matter.begin()`)
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- No additional hardware; temperature and tank state are simulated

For a production device, replace the simulation with real sensors and actuators with appropriate safety interlocks.

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `WiFi` (only for ESP32 and ESP32-S2)

### Configuration

1. **Wi-Fi credentials** (if not using BLE commissioning — mandatory for ESP32 | ESP32-S2):
   ```cpp
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASSWORD "your-password"
   ```

## Building and Flashing

1. Open the example in the Arduino IDE: **File → Examples → Matter → Control → MatterWaterHeaterAdvanced**.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Open the Serial Monitor at **115200**. Wi-Fi connection messages appear only on ESP32 and ESP32-S2. CHIPoBLE targets get the operational network from the hub. `matterWaitUntilReady()` prints the pairing information and waits until the controller CASE session is up. About five seconds later `loop()` starts the simulated heating log:

```
Controller CASE session is up.
Hub Boost / CancelBoost is handled by the endpoint.
Send Boost from the controller to heat faster (even if Mode or System is Off).
CancelBoost, or a timed / one-shot Boost, returns Boost to off and restores any temporary setpoint.
changed | Temp: 20.0 C | Setpoint: 48.0 C | Mode: Manual | System: Heat | Boost: off | Tank: 0 % | Demand: 0x01
tick | Temp: 20.5 C | Setpoint: 48.0 C | Mode: Manual | System: Heat | Boost: off | Tank: 1 % | Demand: 0x01
tick | Temp: 21.0 C | Setpoint: 48.0 C | Mode: Manual | System: Heat | Boost: off | Tank: 3 % | Demand: 0x01
boost-on | Temp: 24.0 C | Setpoint: 48.0 C | Mode: Manual | System: Heat | Boost: on | Tank: 14 % | Demand: 0x01
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

Every five seconds the sketch updates the simulated tank:

- **Manual** and system **Heat**: temperature rises by 0.5 °C toward the heating setpoint.
- **Eco**: rises by 0.25 °C and stops at 40 °C even if the setpoint is higher.
- **Boost**: rises by 1.5 °C toward the current heating setpoint even if system mode or water heater mode is Off. A hub **Boost** command can also apply a temporary setpoint and a duration or one-shot; **CancelBoost** (or the timer / one-shot) prints `boost-off` and restores that setpoint.
- **Off** (water heater mode Off, or system Off, with boost inactive): temperature falls toward 20 °C.

Tank percentage is derived from temperature versus the setpoint. **HeatDemand** is the heater-type bitmap while heating and 0 when the target is reached or heating is off. A line prefixed `changed` is printed as soon as the hub updates mode, system mode, or setpoint. A Boost transition prints `boost-on` or `boost-off`.

### Hub Boost / CancelBoost

After commissioning, send **Boost** from a controller that exposes Water Heater Management commands (Home Assistant Matter / chip-tool). The serial log prints `boost-on`. While Boost is Active this sketch heats at 1.5 °C per tick, even if Mode or System is Off. **CancelBoost**, a duration timeout, or a one-shot that has reached the setpoint (and optional target tank %) prints `boost-off` and restores any temporary setpoint the command applied.

You can also call `waterHeater.setBoostState(MatterWaterHeater::BOOST_ACTIVE)` locally; the same heating path and `boost-on` log apply. There is no Arduino API that sends the Matter Boost command fields (duration, oneShot, temporarySetpoint).

### Smart Home Integration

Use a Matter-compatible hub (Home Assistant, Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

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

1. **`setup()`**: Creates the `MatterWaterHeater` endpoint (`begin()` adds WHM tank features and the Boost/CancelBoost delegate), configures attributes, then `Matter.begin()` and `matterWaitUntilReady()`. Prints how to exercise hub Boost.
2. **`loop()`**: `matterRestartIfNoFabric()`. Prints `changed` when mode, system mode, or setpoint changes, and `boost-on` / `boost-off` when **BoostState** changes. Every five seconds applies Manual, Eco, Boost, or Off behavior, updates **HeatDemand**, and calls `updateTankPercentage()`. `setLocalTemperature()` is what a one-shot Boost uses to finish. There is no button; decommission is not in this sketch.
3. **`updateTankPercentage()`**: Maps temperature between cold water and setpoint to **TankPercentage**.
4. **`logWaterHeater()`**: Prints temperature, setpoint, water heater mode, system mode, boost, tank percent, and heat demand.

For a real appliance, read tank temperature from a sensor, drive the heating element with proper safety interlocks, and never rely on Matter as the only safety layer.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured (ESP32 / ESP32-S2 need Wi-Fi in the sketch or another commissioning path).
- **Tank attributes missing on hub**: Call `waterHeater.begin()` before `Matter.begin()` so EnergyManagement and TankPercent features are provisioned.
- **HeatDemand does not match hub after mode change**: Confirm `MatterWaterHeater` is up to date; controller writes to SystemMode/Boost/HeaterTypes sync HeatDemand in the library.
- **Hub Boost / CancelBoost does nothing**: Confirm `MatterWaterHeater` is up to date. Those commands are handled by the Water Heater Management delegate (`setBoostState()` still works for local on/off).
- **Failed to commission**: Erase flash (**Erase All Flash Before Sketch Upload**) or use another Matter node on the same fabric.
- **No serial output**: Check baud rate (115200) and USB connection.

## Related Documentation

- [Matter Water Heater (basic)](../MatterWaterHeater)
- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter Water Heater Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_water_heater.html)

## License

This example is licensed under the Apache License, Version 2.0.
