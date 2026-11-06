# CoAP Sensor — Native API

Simple telemetry pattern with a read-only CoAP resource and a polling client.
Both sketches share the same hard-coded operational dataset (no Commissioner).

| Sketch | Role |
| --- | --- |
| [sensor_server (Leader + server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) | Leader + CoAP resource `sensor/temp` |
| [sensor_client (polling client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client) | Child that polls temperature every 10 s |

| Aspect | Detail |
| --- | --- |
| Server | `serve()` sugar for a read-only resource; callback mode — no CoAP polling in `loop()` |
| Client | `setConfirmable(false)` — NON GET telemetry (no ACK handshake) |
| Dataset | Leader: full dataset with fixed Extended PAN ID; client: network key only |

## How to Run

1. Flash [sensor_server (Leader + server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) on one
   board and open Serial Monitor at `115200`.
2. Wait for `Ready. Serving GET sensor/temp`.
3. Flash [sensor_client (polling client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client) on a
   second board.
4. The client prints a temperature reading every 10 seconds.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |

## Troubleshooting

**Startup order:** Flash [CoAP Sensor — sensor_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) first and wait for
`Ready. Serving GET sensor/temp`. Then flash [CoAP Sensor — sensor_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client). The
client only needs the same `NETKEY` as the server — reset the client if it
booted before the server was Leader, or erase flash after changing dataset
constants.

| Symptom | Likely cause |
| --- | --- |
| Client `Attach failed` | **Flash `sensor_server` first** until `Ready. Serving GET sensor/temp`. Client needs the same `NETKEY`. |
| Client `Started as Leader` | Server not running — client formed its own network. Start server, erase client flash, retry. |
| Client started before server | No Leader on mesh — start server, then reset client. |
| `GET failed: timeout` | Separate Thread partitions (wrong startup order) or server CoAP not started. |
| Stale temperature on client | Server must call `setResourceValue()` each second in `loop()`. |

## See Also

* [UDP Sensor Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork) — UDP-based telemetry with ACKs.
* [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md) — all Native CoAP demos and port conventions.

## License

Apache License 2.0.
