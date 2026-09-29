# Matter Water Heater Example

This example demonstrates how to create a Matter-compatible water heater device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, water heater attributes (temperature, setpoint, tank state, heat demand), and a simulated heating cycle.

## Supported Targets

| SoC      | This sketch              | CHIPoBLE | Also in prebuild       |
| -------- | ------------------------ | -------- | ---------------------- |
| ESP32    | CHIPoBLE (no Wi-Fi code) | On       | Ethernet (EMAC or SPI) |
| ESP32-S2 | CHIPoBLE (no Wi-Fi code) | Off      | Ethernet (SPI)         |
| ESP32-S3 | CHIPoBLE (hub Wi-Fi)     | On       | Ethernet (SPI)         |
| ESP32-C3 | CHIPoBLE (hub Wi-Fi)     | On       | Ethernet (SPI)         |
| ESP32-C5 | CHIPoBLE (hub)           | On       | Ethernet (SPI)         |
| ESP32-C6 | CHIPoBLE (hub, default)  | On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Thread (hub)             | On       | Ethernet (SPI)         |

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
- Water Heater Management tank attributes (provisioned in `MatterWaterHeater::begin()` before `Matter.begin()`)
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- No additional hardware required; temperature and tank state are simulated in the sketch

For a production device, replace the simulation with real sensors and actuators with appropriate safety interlocks.

## Matter Device Type

The endpoint implements the Matter Water Heater device type:

```text
Water Heater
Device Type ID: 0x050F
```

The endpoint uses the following Matter clusters:

* Descriptor
* Water Heater Management
* Water Heater Mode
* Thermostat

## Running the example

Open the example from the Arduino IDE:

```text
File
  → Examples
    → Matter
      → Control
        → MatterWaterHeater
```

Select an ESP32 board and compile/upload the example.

After startup, the device prints its Matter commissioning information to the serial console.

Open the Serial Monitor at:

```text
115200 baud
```

If the device has not yet been commissioned, the sketch prints the manual pairing code and QR-code URL.

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. Arduino libraries: `Matter`

Before uploading, set **Partition Scheme** to **Huge APP (3 MB No OTA / 1 MB SPIFFS)** and enable **Erase All Flash Before Sketch Upload** (see Matter library documentation).

## Simulated water heater

The example simulates a heating cycle.

The default configuration is:

| Parameter           |             Value |
| ------------------- | ----------------: |
| Tank volume         |             100 L |
| Initial temperature |             20 °C |
| Heating setpoint    |             48 °C |
| Heater type         | Immersion element |
| System mode         |              Heat |
| Water heater mode   |            Manual |
| Tank percentage     |               0 % |

Every few seconds the example increases the simulated water temperature until the configured heating setpoint is reached.

The tank percentage is calculated from the simulated water temperature.

## API

The `MatterWaterHeater` class provides APIs for reading and updating the main Water Heater attributes.

### Temperature

```cpp
waterHeater.setLocalTemperature(45.0f);

float temperature =
    waterHeater.getLocalTemperature();
```

### Heating setpoint

```cpp
waterHeater.setHeatingSetpoint(48.0f);

float setpoint =
    waterHeater.getHeatingSetpoint();
```

### System mode

```cpp
waterHeater.setSystemMode(
    MatterWaterHeater::SYSTEM_MODE_HEAT
);
```

### Heater type

```cpp
waterHeater.setHeaterTypes(
    MatterWaterHeater::IMMERSION_ELEMENT_1
);
```

### Tank volume

```cpp
waterHeater.setTankVolume(100);
```

### Tank percentage

```cpp
waterHeater.setTankPercentage(75);
```

### Water Heater mode

```cpp
waterHeater.setWaterHeaterMode(
    MatterWaterHeater::WATER_HEATER_MODE_MANUAL
);
```

### Heat demand

`HeatDemand` is a bitmap of the heat sources (same bit values as `HeaterTypes`) currently active, not a percentage. `setSystemMode()` and `setBoostState()` keep it in sync automatically: switching to `SYSTEM_MODE_HEAT` (or activating Boost) sets it to the configured heater types, and switching to `SYSTEM_MODE_OFF` (with Boost inactive) clears it.

Call `setHeatDemand()` directly only to report finer-grained state while `SystemMode` stays `Heat` - for example, turning the reported demand off once the setpoint has been reached, as this example's `loop()` does:

```cpp
waterHeater.setHeatDemand(
    waterHeater.getHeaterTypes()
);

waterHeater.setHeatDemand(0);
```

### Boost

```cpp
waterHeater.setBoostState(
    MatterWaterHeater::BOOST_ACTIVE
);
```

## Implementation notes

The Water Heater endpoint is implemented using the `esp-matter` data model provided by Arduino-ESP32.

`MatterWaterHeater::begin()` must be called **before** `Matter.begin()`. It provisions Water Heater Management **EnergyManagement** and **TankPercent** features so `TankVolume` and `TankPercentage` exist on the endpoint. Configure heater type, tank size, setpoints, and modes after `begin()` and before or after `Matter.begin()` (tank setters work once features are added).

The Arduino API is intentionally kept at a higher level than the underlying Matter data model. Applications should normally use `MatterWaterHeater` rather than manipulating the Matter clusters directly.

The example is intended as a starting point for applications implementing a physical water heater, boiler, heat-pump water heater, or similar appliance.

## Limitations

This example uses simulated values and does not control physical heating hardware.

In a production implementation:

1. Read the actual tank temperature from a sensor.
2. Update the Matter local temperature attribute.
3. Apply the Matter heating setpoint to the physical controller.
4. Update the tank percentage from the actual tank state.
5. Report the actual heat demand.
6. Implement the appropriate safety limits and hardware interlocks.

Never use the Matter endpoint as the only safety mechanism for controlling a real heating element.

## Related Matter specification

The implementation follows the Matter Water Heater device model and its associated Water Heater Management, Water Heater Mode and Thermostat clusters.

For additional information, refer to the Matter specification and the Arduino-ESP32 Matter documentation.
