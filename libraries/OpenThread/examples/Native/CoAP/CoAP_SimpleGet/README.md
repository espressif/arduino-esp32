# CoAP Simple GET — Native API

Minimal two-board demo: one node runs a CoAP server with path `hello`, the other
sends a confirmable GET and prints the payload on Serial. The Leader forms the
network with a fixed dataset; the client joins using the network key only (no
Commissioner).

| Sketch | Role |
| --- | --- |
| [server (Leader + CoAP server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server) | Thread Leader + CoAP server on port 5683 |
| [client (one-shot GET)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client) | Thread Child + one-shot GET client |

| Parameter | Value |
| --- | --- |
| Network name | `ESP_OT_CoAP_Demo` |
| Channel | 24 |
| PAN ID | `0xC0A0` |
| CoAP path | `hello` |

The server registers `onHello()` in callback mode (no CoAP polling in `loop()`).
The client uses `setConfirmable(true)` for a reliable GET with response.

## How to Run

1. Flash [server (Leader + CoAP server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server) on one ESP32-H2 / ESP32-C6 /
   ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait until Serial shows `Attached as Leader` and `CoAP server listening on path 'hello'`.
3. Flash [client (one-shot GET)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client) on a second board (same
   `NETKEY` as the server).
4. The client performs one GET on `hello` and prints the response.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |

## Troubleshooting

**Startup order:** Flash [CoAP SimpleGet — server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server) first and wait for `Attached as Leader`
and `CoAP server listening on path 'hello'`. Then flash [CoAP SimpleGet — client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client). The
client only needs the same `NETKEY` as the server — reset the client if it booted
before the server was Leader, or erase flash after changing dataset constants.

| Symptom | Likely cause |
| --- | --- |
| Client `Thread attach failed` | **Flash `server` first** and wait for Leader. Client needs the same `NETKEY`. |
| Client `Started as Leader` | Server not running — client formed its own network. Start server, erase client flash, retry. |
| Client started before server | No Leader on the mesh — start server, then reset client. |
| `GET hello -> error timeout` | Separate Thread partitions (wrong startup order) or server CoAP not started. |
| Wrong payload or attach to wrong network | Stale NVS dataset on either board — erase flash on both boards. |

## See Also

* [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) — multicast lamp control over CoAP.
* [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md) — all Native CoAP demos and port conventions.
* [CLI CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) — CLI-based CoAP demos for comparison.

## License

Apache License 2.0.
