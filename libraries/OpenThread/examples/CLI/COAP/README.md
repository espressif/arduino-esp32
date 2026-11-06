# CoAP Lamp / Switch — CLI

A two-board CoAP demo driven through the **OpenThread CLI** (`coap ...` commands
issued from the sketch via `OThreadCLI`). A switch toggles a remote RGB-LED lamp
over the Thread mesh using CoAP PUT requests.

| Sketch | Role |
| --- | --- |
| [CLI CoAP lamp (Leader + server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp) | Leader + CoAP **server**; registers a `Lamp` resource and drives the RGB LED on PUT |
| [CLI CoAP switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch) | Router/Child + CoAP **client**; sends PUT `0`/`1` to toggle the lamp on a button press |

Both sketches must share the same network key, channel, multicast address, and
CoAP resource name.

## CLI vs Native CoAP Light Switch

This CLI pair is the string-command counterpart of
[CoAP Light Switch (Native API)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch). Constants
**differ by design** — do not mix CLI and Native boards on one test without
aligning all of the following:

| Parameter | CLI (`coap_lamp` / `coap_switch`) | Native (`light` / `switch`) |
| --- | --- | --- |
| Channel | **24** | **15** |
| Multicast | **`ff05::abcd`** (site-local) | **`ff03::abcd`** (realm-local) |
| Join model | CLI dataset in sketch | Native Commissioner + Joiner (PSKd) on `switch` |

Use one family per mesh test. See Native README for typed `OThreadCoAP*` APIs.

## How to Run

1. Flash [CLI CoAP lamp (Leader + server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp) on one ESP32-H2 / ESP32-C6 / ESP32-C5 board
   (needs an RGB LED). Its LED turns **green** when ready.
2. Flash [CLI CoAP switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch) on a second board with the **matching**
   network key, channel, multicast address, and resource name. Its LED turns
   **blue** when ready.
3. Press the BOOT button on the switch to toggle the lamp ON/OFF.

Each sketch has its own README with full configuration, expected output, and code
walkthrough.

## Troubleshooting

**Startup order:** Flash [CLI CoAP lamp (Leader + server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp) first and wait until the RGB LED turns **green** (Leader ready). Then flash [CLI CoAP switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch) with matching network key, channel, multicast address, and resource name. Reset the switch if it booted before the lamp was ready.

| Symptom | Likely cause |
| --- | --- |
| Switch LED stays red | Lamp (Leader) not running yet or credentials mismatch — start lamp first, then reset switch. |
| Button press does not toggle lamp | Network key, channel, `OT_MCAST_ADDR`, or `OT_COAP_RESOURCE_NAME` mismatch between boards. |
| CoAP request timeout | Lamp not listening, switch not attached, or switch started before lamp formed the network — reset switch after lamp is green. |
| Lamp LED stays red | Leader setup failed — check Serial for CLI errors; clear NVS if a stale dataset blocks Leader role. |

## See Also

- [Native CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP) — the same CoAP scenarios using the typed Native API (`OThreadCoAPClient` / `OThreadCoAPServer`), no CLI string parsing. Start with [CoAP Light Switch (Native API)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch).
- [CLI examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/CLI/README.md) — CLI vs Native overview.

## License

Apache License 2.0.
