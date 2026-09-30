# Matter Water Heater Example

This is the basic water heater sketch: commission a Matter Water Heater, report tank temperature, and heat toward the setpoint.\
It honors System Mode, Water Heater Mode Off, and Boost, and clears **HeatDemand** when the tank is not drawing power.\
Eco, Boost duration / one-shot details, tank percentage, and richer logging are in [MatterWaterHeaterAdvanced](../MatterWaterHeaterAdvanced).

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
- Heats while System Mode is Heat and Water Heater Mode is not Off, or while a hub Boost is Active (Boost still heats if Mode or System is Off)
- Cools toward 20 °C when heating is off
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

Open the Serial Monitor at **115200**. `matterWaitUntilReady()` prints the pairing code and waits until the controller CASE session is up. About five seconds later `loop()` prints the simulated tank:

```
Controller CASE session is up.
Temp: 20.5 C | Setpoint: 48.0 C | Heat: on
Temp: 21.0 C | Setpoint: 48.0 C | Heat: on
```

`Heat: on` is **HeatDemand** (element drawing power), not System Mode. At the setpoint the log shows `Heat: off`.

Change the setpoint, System Mode, or Water Heater Mode from the hub; the next tick follows the new values. Mode **Off** stops heating unless Boost is Active.

## Using the Device

The endpoint is Matter device type **0x050F**. This sketch drives temperature and HeatDemand only (no Eco cap, tank %, or Boost session log):

| Parameter           |  Value |
| ------------------- | -----: |
| Initial temperature |  20 °C |
| Heating setpoint    |  48 °C |
| System mode         |   Heat |
| Water heater mode   | Manual (library default) |

Every five seconds the temperature rises by 0.5 °C toward the setpoint while heating. It falls toward 20 °C when System Mode is Off or Water Heater Mode is Off, unless Boost is Active. **HeatDemand** is the heater-type bitmap while the element is on, and 0 at the setpoint or when heating is off.

Use a Matter hub (Home Assistant, Apple Home, Amazon Alexa, or Google Home) to scan the QR code or enter the manual pairing code from the Serial Monitor.

## Code Structure

1. **`setup()`**: `waterHeater.begin()` (before `Matter.begin()`), set temperature and setpoint, then `Matter.begin()` and `matterWaitUntilReady()`.
2. **`loop()`**: `matterRestartIfNoFabric()`. Every five seconds updates temperature and **HeatDemand**, and prints them. Mode Off and System Off stop heating unless Boost is Active.

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
