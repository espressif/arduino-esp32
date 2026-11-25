# Arduino-ESP32 Zigbee HVAC Example

This example creates a Zigbee end device that implements the HA Thermostat cluster (0x0201, revision 3). A coordinator such as Home Assistant (ZHA) or Zigbee2MQTT can set the system mode and the occupied heating and cooling setpoints. Relative changes use the SetpointRaiseOrLower command.

The example simulates a room instead of reading a sensor. LocalTemperature starts at 18°C, below the 20°C heating setpoint, and moves 0.5°C per second toward the occupied setpoint while the mode is heating or cooling. Auto leaves the temperature alone while it is already between the two setpoints, so picking Auto from inside that band first steps the room just below the heating setpoint. Cool does the same above the cooling setpoint. Replace `simulate_room()` with the real sensor when you have one.

# Supported Targets

| Supported Targets | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

## HVAC Functions

1. Join the network as a thermostat end device.
2. Accept system mode and occupied setpoint writes.
3. Accept SetpointRaiseOrLower.
4. Publish the simulated room temperature and running state.

The constructor marks running state reportable. ``LocalTemperature`` already is. System mode and the occupied setpoints stay unreportable. The stack answers those writes itself, and it reports local temperature and running state on the interval ZHA configures. ``onHVACChange()`` only stores the new values. ``loop()`` moves the simulated room when the mode changes and updates ``LocalTemperature``. A short BOOT press sends that report immediately. A setpoint write does not move the room.

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

A short press of the BOOT button raises the simulated room by one degree more than the gap between the heating and cooling setpoints (7°C with the defaults of 20°C and 26°C). From anywhere inside that band, one press lands above the cooling setpoint. Hold BOOT for about a second and release to step the same amount down, below the heating setpoint. Hold it for 3 seconds to factory-reset the Zigbee stack.

### Configure the Project

#### Using Arduino IDE

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`.
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`.
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.

## Troubleshooting

If the device does not join, erase the flash before uploading the sketch again.

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.

The coordinator network is closed after reboot. Open it from the coordinator, or call `Zigbee.openNetwork(time)` on a coordinator sketch.

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***
