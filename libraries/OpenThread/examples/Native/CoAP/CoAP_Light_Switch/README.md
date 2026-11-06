# CoAP Light Switch — Native API

Native-API rewrite of [CLI CoAP lamp (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp) and
[CLI CoAP switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch). Uses Thread commissioning for network
admission and plain CoAP on port **5683** for lamp control.

| Sketch | Role | CoAP behavior |
| --- | --- | --- |
| [light (Leader + lamp server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) | Leader + Commissioner + lamp server | GET/PUT on path `Lamp`; multicast PUT acts without replying |
| [switch (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch) | Joiner client | fire-and-forget NON PUT `0`/`1` on button press |

The switch sends **non-confirmable** multicast PUT to realm-local group
`ff03::abcd`. The light subscribes to that group and does not reply to multicast
commands (RFC 7252 §8.2) — this scales cleanly to many switches and lamps.

Compared to the CLI version: the CLI sketch sends confirmable multicast and
parses `"coap response from..."`, which is convenient but not strictly correct
(RFC 7252 §8.1 forbids confirmable multicast). The CLI demos use realm-local
group `ff05::abcd` on channel **24**; these Native sketches use `ff03::abcd`
on channel **15** — do not mix CLI and Native boards on the same test without
aligning network parameters and multicast scope.

## How to Run

1. Flash [light (Leader + lamp server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) on one ESP32-H2 / ESP32-C6 /
   ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait for `Commissioner ready (PSKd "J01NME")` and
   `Ready. CoAP resource 'Lamp' (multicast group ff03::abcd)`.
3. Flash [switch (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch) on one or more additional boards.
4. Press **BOOT** on a switch to toggle the lamp via CoAP PUT.

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

**Startup order:** Flash [CoAP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) first and wait for `Commissioner ready`.
Then flash [CoAP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch). Reset the switch if it joined before Commissioner
was active.

| Symptom | Likely cause |
| --- | --- |
| Switch `startJoiner failed` | **Flash `light` first** and wait for `Commissioner ready`. Reset switch after light is up. |
| Switch attached but lamp unchanged | Light not subscribed to multicast group, wrong `LAMP_GROUP`, or light not running. |
| CLI used CON to multicast | Native sketch uses NON + no reply — see group overview above. |
| Server reset after switch joined | Light forms a new network each boot (`initNew()`) — reset switch to re-join. |

## See Also

* [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md) — all Native CoAP demos and port conventions.
* [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) — same lamp pattern over UDP port 5051.
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — Joiner / Commissioner without CoAP.
* [CLI CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) — CLI-based CoAP lamp/switch demos.

## License

Apache License 2.0.
