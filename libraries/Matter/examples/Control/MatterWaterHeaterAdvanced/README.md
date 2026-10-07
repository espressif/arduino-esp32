# Matter Water Heater Advanced Example

This example shows every Arduino-facing Water Heater feature: mode, Eco, Boost / CancelBoost, tank percentage, estimated heat required, and HeatDemand.\
For a shorter sketch (System Mode, Boost, target temperature, Serial controller messages), start with [MatterWaterHeater](../MatterWaterHeater).

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
- Simulated tank temperature, heating setpoint, tank percentage, estimated heat required, and heat demand
- Water Heater Mode (Off / Manual / Eco) and Thermostat System Mode (Off / Heat)
- Controller **Boost** / **CancelBoost** (duration, one-shot, optional temporary setpoint)
- Serial `Controller:` lines for Boost, system mode, water heater mode, and target temperature (same as the basic example)
- `MatterWaterHeater::begin()` provisions Water Heater Management **EnergyManagement** and **TankPercent** (call before `Matter.begin()`)
- After `begin()`, the sketch sets the controller setpoint dial to **25 °C … 60 °C** (`setAbsoluteMinimumHeatingSetpoint()` / `setAbsoluteMaximumHeatingSetpoint()`, then `Min` / `Max`). The API cannot go outside the library span **20 °C … 85 °C**. Tank temperature is independent (starts at 20 °C, cools to 16 °C).
- Matter commissioning via QR code or manual pairing code

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
Ready | operation: Heat | target: 48.0 C | water heater mode: Manual
ready | Temp: 20.0 C | Setpoint: 48.0 C | Op: Heat | Mode: Manual | System: Heat | Boost: off | Tank: 0 % | HeatReq: 3.256 kWh | HeatDemand: Boiler
tick | Temp: 20.5 C | Setpoint: 48.0 C | Op: Heat | Mode: Manual | System: Heat | Boost: off | Tank: 1 % | HeatReq: 3.198 kWh | HeatDemand: Boiler
Controller: Boost on
Operation: Boost
boost-on | Temp: 24.0 C | Setpoint: 48.0 C | Op: Boost | Mode: Manual | System: Heat | Boost: on | Tank: 14 % | HeatReq: 2.791 kWh | HeatDemand: Immersion 1+Immersion 2+Boiler
Controller: target temperature 42.0 C
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
| Cold-water floor    |             16 °C |
| Setpoint dial       |     25 °C … 60 °C |
| Heating setpoint    |             48 °C |
| Heater types        | All five WHM bits |
| System mode         |              Heat |
| Water heater mode   |            Manual |
| Tank percentage     |               0 % |
| Estimated heat      |         3.256 kWh |

Every five seconds the sketch updates the simulated tank:

- **Manual** and System Mode **Heat**: +0.5 °C toward the heating setpoint. **HeatDemand** is **Boiler**.
- **Water Heater Mode Eco**: +0.25 °C, stops at 40 °C even if the setpoint is higher, **HeatDemand** is **Heat pump**.
- **Boost**: +1.0 °C (double Manual) toward the current heating setpoint even if system mode or water heater mode is Off. **HeatDemand** is **Boiler+Immersion 1+Immersion 2**. A controller **Boost** command can also apply a temporary setpoint and a duration or one-shot; **CancelBoost** (or the timer / one-shot) restores that setpoint.
- **Off** (water heater mode Off, or system Off, with boost inactive): temperature falls at 0.25 °C/s toward 16 °C.

The **25 °C … 60 °C** dial is `OccupiedHeatingSetpoint` only. `LocalTemperature` can sit below the dial minimum (this sketch starts at 20 °C). A controller that still shows **20 °C … 85 °C** after a flash may be using the range from first commissioning; re-interview the node so it re-reads `AbsMin` / `AbsMax`.

Tank percentage is derived from temperature versus the setpoint. **EstimatedHeatRequired** is the remaining energy to reach the heating goal (`volume × ΔT × 1.163 Wh/L·°C`, stored as mWh). Eco uses the 40 °C cap as the goal (less energy than Manual/Boost at the same tank temperature). Off still reports energy if the tank is below that goal; the value falls as the tank heats and is 0 at the goal. Reaching the target during **Boost** ends Boost so System Mode Heat continues. **Off** is only when the controller writes System Mode Off. Controller writes print `Controller:` lines immediately (Boost, system mode, water heater mode, target temperature), then a `changed` / `boost-on` / `boost-off` snapshot.

### HeatDemand (Serial names)

**HeatDemand** is not Eco, Manual, Boost, or Off. Those are Water Heater Mode, System Mode, and Boost. HeatDemand is the Matter bitmap of **which heat sources are drawing power now** (the same bits as `HeaterTypes`).

`HeaterTypes` advertises every Matter source bit. `heatDemandName()` names them: `Immersion 1` (`0x01`), `Immersion 2` (`0x02`), `Heat pump` (`0x04`), `Boiler` (`0x08`), `Other` (`0x10`). Several bits print as `Name+Name`.

This sketch uses a **boiler** as the everyday heater (common on the market). While the tank is below the goal, **HeatDemand** is a subset of `HeaterTypes`:

| Serial                           | Matter value  | When                                        |
| -------------------------------- | ------------- | ------------------------------------------- |
| `Boiler`                         |        `0x08` | Manual Heat, tank below the goal            |
| `Heat pump`                      |        `0x04` | Water Heater Mode Eco, tank below the goal  |
| `Immersion 1+Immersion 2+Boiler` |        `0x0B` | Boost, tank below the goal                  |
| `idle`                           |           `0` | At the goal, or not heating (Off, no Boost) |

`Other` is in `HeaterTypes` so the helper has a name for that bit; this sketch does not turn it on. Eco / Manual / Boost change **which subset** is on, not the meaning of HeatDemand (it is still “sources drawing power now”).

The [basic example](../MatterWaterHeater) prints the same attribute as `on` / `idle` and does not name the heater type.

### Hub Boost / CancelBoost

After commissioning, send **Boost** from a Matter controller that exposes Water Heater Management commands. The serial log prints `Controller: Boost on` and `boost-on`. While Boost is Active this sketch heats at 1.0 °C per tick (double Manual), even if Mode or System is Off. **CancelBoost**, a duration timeout, or a one-shot that has reached the setpoint (and optional target tank %) prints `Controller: Cancel Boost` / `boost-off` and restores any temporary setpoint the command applied.

You can also call `waterHeater.setBoostState(MatterWaterHeater::BOOST_ACTIVE)` locally; the same heating path and log apply. There is no Arduino API that sends the Matter Boost command fields (duration, oneShot, temporarySetpoint).

### Commissioning

Use a Matter controller to scan the QR code or enter the manual pairing code from the Serial Monitor.

## Code Structure

1. **`setup()`**: Creates the `MatterWaterHeater` endpoint (`begin()` adds WHM tank features, default **20 °C … 85 °C** setpoint limits, and the Boost/CancelBoost delegate), then sets the dial to **25 °C … 60 °C**, tank attributes, modes, and initial **EstimatedHeatRequired**. `Matter.begin()` and `matterWaitUntilReady()`. Prints how to exercise controller Boost.
2. **`loop()`**: `matterRestartIfNoFabric()`. Prints `Controller:` lines and a snapshot when Boost, system mode, water heater mode, or setpoint changes. Every five seconds applies Manual, Water Heater Mode Eco, Boost, or Off behavior, updates **HeatDemand**, `updateTankPercentage()`, and `updateEstimatedHeatRequired()`. `setLocalTemperature()` is what a one-shot Boost uses to finish. There is no button; decommission is not in this sketch.
3. **`updateTankPercentage()`**: Maps temperature between cold water and setpoint to **TankPercentage**.
4. **`heatingGoalC()` / `updateEstimatedHeatRequired()`**: Eco caps the goal at 40 °C; otherwise the goal is the heating setpoint. Remaining energy is `volume × ΔT × 1163 mWh/L·°C`.
5. **`activeHeatSources()` / `heatDemandName()` / `logWaterHeater()`**: While heating, Manual reports **Boiler**, Eco **Heat pump**, Boost **Boiler+Immersion 1+Immersion 2**. Serial names every WHM bit (see **HeatDemand (Serial names)**), not hex.

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
