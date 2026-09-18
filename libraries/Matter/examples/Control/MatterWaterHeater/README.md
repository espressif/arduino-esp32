# Matter Water Heater

This example demonstrates how to create and control a Matter Water Heater endpoint using the Arduino-ESP32 Matter library.

The example exposes a Matter Water Heater device and demonstrates:

* Local water temperature
* Heating setpoint
* Heating system mode
* Water heater operating mode
* Heater type
* Heat demand
* Tank volume
* Tank percentage
* Boost state

## Hardware

This example requires an ESP32 board supported by the Arduino-ESP32 Matter library.

No additional hardware is required to run the example. The water temperature and tank state are simulated by the sketch.

For a real water heater implementation, the simulated values should be replaced by readings from the appropriate sensors and the Matter attributes should be updated accordingly.

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
      → MatterWaterHeater
```

Select an ESP32 board and compile/upload the example.

After startup, the device prints its Matter commissioning information to the serial console.

Open the Serial Monitor at:

```text
115200 baud
```

If the device has not yet been commissioned, the sketch prints the manual pairing code and QR-code URL.

## Commissioning

Use a Matter controller such as:

* Google Home
* Apple Home
* Amazon Alexa
* Home Assistant

to commission the device.

Once commissioned, the Water Heater endpoint can be controlled through the Matter controller.

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

```cpp
waterHeater.setHeatDemand(100);
```

### Boost

```cpp
waterHeater.setBoostState(
    MatterWaterHeater::BOOST_ACTIVE
);
```

## Implementation notes

The Water Heater endpoint is implemented using the `esp-matter` data model provided by Arduino-ESP32.

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
