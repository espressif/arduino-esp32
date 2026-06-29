# UDP Light + Switch over Thread — Native API

This folder contains a two-board "smart light" example built with the Arduino
OpenThread Native API (`OThread` + `OThreadUDP`). The pair uses Thread
commissioning for network admission and raw IPv6 UDP packets for application
traffic.

| Sketch | Role |
| ------ | ---- |
| [light (Leader + lamp server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) | Server: Thread **Leader + Commissioner** and UDP-controlled RGB light. |
| [switch (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch) | Client: Thread **Joiner** and UDP switch that toggles the light. |

One `light` can serve many `switch` nodes. The light forms or resumes the Thread
network, opens the Commissioner join window for PSKd `J01NME`, listens on
realm-local multicast `ff03::abcd` on UDP port **5051**, and sends a unicast
`ACK ON` / `ACK OFF` back to the switch that sent each command.

The example uses UDP port **5051** intentionally. Avoid **5683** / **5684**
(CoAP/CoAPs) and **61631** (Thread TMF CoAP), because those ports are used by
OpenThread internals.

## How to Run

1. Flash [light (Leader + lamp server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) on one ESP32-H2 / ESP32-C6 /
   ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait for the light to become Leader and print that the Commissioner is
   active.
3. Flash [switch (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch) on one or more additional
   boards.
4. Press the **BOOT** button on a switch to send `TOGGLE` to the light.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `light` |
| `CONFIG_OPENTHREAD_JOINER=y` | `switch` |

## Troubleshooting

**Startup order:** Flash [light (Leader + lamp server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) first and wait for
Leader + Commissioner active. Then flash [switch (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch).
If a switch booted before the light was ready, press **reset** on the switch
after the light prints Commissioner active.

| Symptom | Likely cause |
| --- | --- |
| Switch `startJoiner failed` | Light not running or Commissioner join window closed — start light first, wait for Commissioner, then reset switch. |
| Switch attached but lamp unchanged | Light not listening on `ff03::abcd`, wrong UDP port, or light not running. |
| Works after first boot, fails on reset | Light resumes NVS network; switch resumes NVS or re-joins — reset switch if it joined before Commissioner was ready on first commission. |
| Multicast traffic but no ACK | Unicast ACK goes back to switch RLOC — check light Serial for incoming `TOGGLE`. |

## See Also

* [Native UDP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/README.md) — all Native UDP demos and port conventions.
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — same Joiner /
  Commissioner flow without UDP traffic.
* [UDP Sensor Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork) — many-to-one UDP telemetry
  with sequence ACKs.
* [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) — same lamp
  pattern over CoAP port 5683.

## License

Apache License 2.0.
