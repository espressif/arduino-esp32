# Native UDP Examples

IPv6 UDP over Thread using `OThreadUDP` (raw `otUdpSocket`, no lwIP). These
examples use application ports **5050** and **5051** — not OpenThread-reserved
CoAP ports 5683/5684 or TMF port 61631.

| Folder | Sketches | Topic |
| --- | --- | --- |
| [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) | [light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light), [switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch) | Joiner / Commissioner + multicast lamp on port 5051 |
| [UDP Sensor Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork) | [sensor_collector (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector), [sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) | Many-to-one telemetry with sequence ACKs on port 5050 |

## How to Run

1. Pick a demo folder above and open its group README for scenario-specific steps.
2. Flash the **Leader / Commissioner / collector / light** sketch first and wait
   until Serial reports attached (+ Commissioner active when joiners are used).
3. Flash the **Joiner / switch / sensor** sketch on one or more additional boards.
4. Open Serial Monitor at **115200** on each board and follow the per-sketch README.

Each sketch folder has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | All UDP examples |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | All UDP examples |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `light`, `sensor_collector` |
| `CONFIG_OPENTHREAD_JOINER=y` | `switch`, `sensor_node` |

## Troubleshooting

**Startup order:** Flash the **Leader / Commissioner / collector / light** sketch
first and wait until Serial reports attached (and Commissioner active when
joiners are used). Then flash or reset **Joiner / switch / sensor** boards. If a
client booted before the server was ready, press **reset** on the client after the
server is up.

| Symptom | Likely cause |
| --- | --- |
| Joiner or sensor cannot attach | **Start the light / collector first.** Wait for `Commissioner ACTIVE` or `Collector listening on UDP port 5050`, then reset the Joiner/switch/sensor. |
| Client attached but no UDP traffic | Server not listening yet, wrong port (`5050`/`5051`), or client targets stale Leader RLOC after server reset — reset the client. |
| Switch toggles once, then fails | Light may have resumed NVS network; switch may need reset if it joined before Commissioner was ready on first boot. |
| Sensor `NO_ACK` or collector silent | Dataset/PSKd mismatch, collector not bound, or sensor started before collector was Leader — start collector first, then reset sensor. |
| Works once, fails after reboot | Check NVS resume behavior — commissioned nodes reattach from stored dataset; joiner window needed only for first commissioning or erased NVS. |

## See also

* [Native examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md) — Native API overview and all example categories.
* [Native CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP) — same IoT patterns over CoAP port 5683 (and CoAPS 5684).
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — Joiner / Commissioner without UDP.

## License

Apache License 2.0.
