# Matter Water Heater Example

This is the basic water heater sketch: commission a Matter Water Heater, report tank temperature, and heat toward the setpoint.\
System Mode, **Boost** / **CancelBoost**, and target temperature are logged on the Serial Monitor as soon as the controller writes them.\
Water Heater Mode Eco, Boost duration / one-shot details, tank percentage, and richer logging are in [MatterWaterHeaterAdvanced](../MatterWaterHeaterAdvanced).

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
- Simulated tank temperature and heating setpoint
- System Mode **Heat** (no Boost): +0.5 °C / 5 s; **Boost**: +1.0 °C / 5 s
- Cools at 0.25 °C/s toward 16 °C when operation is Off (and Boost is inactive)
- Serial lines for Boost / Cancel Boost, system mode, water heater mode, and target temperature
- Clears **HeatDemand** when the tank is at the setpoint or not heating
- Matter commissioning via QR code or manual pairing code

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- No additional hardware; temperature is simulated

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

1. Open the example in the Arduino IDE: **File → Examples → Matter → Control → MatterWaterHeater**.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Open the Serial Monitor at **115200**. `matterWaitUntilReady()` prints the pairing code and waits until the controller CASE session is up. Hub writes print immediately; the tank tick is every five seconds:

```
Controller CASE session is up.
Ready | operation: Heat | target: 48.0 C | water heater mode: Manual
Temp: 20.5 C | Setpoint: 48.0 C | HeatDemand: on | Heat
Controller: Boost on
Operation: Boost
Controller: target temperature 42.0 C
Target reached. HeatDemand idle; leaving Boost for Heat.
Controller: Cancel Boost
Operation: Heat
Temp: 42.0 C | Setpoint: 42.0 C | HeatDemand: idle | Heat
```

`HeatDemand: on` is the Matter **HeatDemand** attribute (which heater types are drawing power: immersion, heat pump, boiler, …). Reaching the setpoint in **Heat** idles HeatDemand and keeps System Mode Heat. Reaching it during **Boost** ends Boost so heating continues in Heat. **Off** is only when the controller writes System Mode Off (that cools the tank).

A controller **Boost** command may use a long duration and no one-shot; this sketch ends that Boost at the setpoint so Boost does not stay active after the tank is hot.

## Using the Device

The endpoint is Matter device type **0x050F**. This sketch drives temperature and HeatDemand (no Eco temperature cap, tank %, or Boost session fields):

| Parameter           |                      Value |
| ------------------- | -------------------------: |
| Initial temperature |                      16 °C |
| Heating setpoint    |                      48 °C |
| System mode         |                       Heat |
| Water heater mode   | Manual (`begin()` default) |

Every five seconds Heat rises 0.5 °C and Boost rises 1.0 °C toward the setpoint. Off falls 0.25 °C/s toward 16 °C unless Boost is Active. **HeatDemand** is the heater-type bitmap while a heater type is on, and 0 at the setpoint or when heating is off.

`MatterWaterHeater::begin()` adds Thermostat **AbsMinHeatSetpointLimit** and **AbsMaxHeatSetpointLimit** (default **20 °C … 85 °C**) so controllers can show a setpoint dial. That range is not the tank temperature: this sketch cools to 16 °C. After `begin()`, `setAbsoluteMinimumHeatingSetpoint()` / `setAbsoluteMaximumHeatingSetpoint()` (and `setMinimumHeatingSetpoint()` / `setMaximumHeatingSetpoint()`) can change the dial within 20 °C … 85 °C. See `ep_water_heater.rst`.

Use a Matter controller to scan the QR code or enter the manual pairing code from the Serial Monitor.

## Code Structure

1. **`setup()`**: `waterHeater.begin()` (before `Matter.begin()`), set temperature and setpoint, then `Matter.begin()` and `matterWaitUntilReady()`.
2. **`loop()`**: `matterRestartIfNoFabric()`. Prints hub changes (Boost, system mode, water heater mode, target temperature) immediately. Every five seconds updates temperature and **HeatDemand**. Off stops heating unless Boost is Active.

There is no button; decommission is not in this sketch.

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured (ESP32 / ESP32-S2 need Wi-Fi in the sketch or another commissioning path).
- **Failed to commission**: Erase flash (**Erase All Flash Before Sketch Upload**) or use another Matter node on the same fabric.
- **No serial output**: Check baud rate (115200) and USB connection.

## Related Documentation

- [Matter Water Heater Advanced](../MatterWaterHeaterAdvanced)
- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Water Heater Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_water_heater.html)

## License

This example is licensed under the Apache License, Version 2.0.
