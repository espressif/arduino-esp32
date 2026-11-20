# Arduino-ESP32 Zigbee HVAC Example

This example creates a Zigbee router that implements the HA Thermostat cluster (0x0201, revision 3). A coordinator such as Home Assistant (ZHA) or Zigbee2MQTT can set the system mode and the occupied heating and cooling setpoints. Relative changes use the SetpointRaiseOrLower command.

The example publishes the chip temperature as LocalTemperature and updates ThermostatRunningState from that reading. Replace `temperatureRead()` with the room sensor the thermostat should report.

# Supported Targets

| Supported Targets | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

## HVAC Functions

1. Join the network as a thermostat router.
2. Accept system mode and occupied setpoint writes.
3. Accept SetpointRaiseOrLower.
4. Publish local temperature and running state.

Occupied cooling is kept at least 2.5°C above occupied heating. Weekly schedule, setback and air-conditioner attributes are not part of this endpoint. Fan Control is a separate cluster and is not on this endpoint.

## Hardware Required

* ESP32-C5, ESP32-C6, ESP32-H2 or ESP32-S31 development board
* A USB cable for power supply and programming
* RGB LED for the current system mode (built-in on most development boards)

The LED colors are:

- OFF: dark
- HEAT: red
- COOL: blue
- AUTO: green
- FAN ONLY: white
- DRY: cyan
- SLEEP: dim blue

Hold the BOOT button for 3 seconds to factory-reset the Zigbee stack.

### Configure the Project

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select Zigbee mode: `Tools -> Zigbee mode: Zigbee ZCZR (coordinator/router)`.
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee ZCZR 4MB with spiffs`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

## Troubleshooting

If the device does not join, erase the flash before uploading the sketch again.

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.

The coordinator network is closed after reboot. Open it from the coordinator, or call `Zigbee.openNetwork(time)` on a coordinator sketch.

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***
