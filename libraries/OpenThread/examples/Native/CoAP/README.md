# Native CoAP Examples

Application CoAP over Thread using `OThreadCoAPClient`, `OThreadCoAPServer`,
`OThreadCoAPSecureClient`, and related classes. Plain CoAP uses UDP port
**5683**; CoAPS (DTLS) uses port **5684** when enabled at build time.

CLI-based CoAP demos remain under [CLI CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) for
reference; these Native examples use typed method calls instead of CLI strings.

| Folder | Sketches | Topic |
| --- | --- | --- |
| [CoAP SimpleGet](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet) | [server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server), [client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client) | Minimal GET on path `hello` |
| [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) | [light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light), [switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch) | Multicast lamp (CLI `coap_lamp` rewrite) |
| [CoAP Sensor](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor) | [sensor_server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server), [sensor_client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client) | `serve()` + NON GET telemetry |
| [CoAP CRUD](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD) | [notes_server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server), [notes_client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client) | `OThreadCoAPResourceStore` REST notes |
| [CoAP Secure](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure) | [secure_server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server), [secure_client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client) | CoAPS with PSK |
| [CoAP Greenhouse](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse) | [greenhouse_server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server), [greenhouse_client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client) | Plain + CoAPS; CON vs NON |

## How to Run

1. Pick a demo folder above and open its group README for scenario-specific steps.
2. Flash the **server / Leader** sketch first and wait until it reports attached +
   Commissioner active (when commissioning is used).
3. Flash the **client / Joiner** sketch on one or more additional boards.
4. Open Serial Monitor at **115200** on each board and follow the per-sketch README
   for expected output and interaction.

Each sketch folder has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | All CoAP examples |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | All CoAP examples |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | Leader + Commissioner sketches |
| `CONFIG_OPENTHREAD_JOINER=y` | Joiner client sketches |
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | `CoAP_Secure`, `CoAP_Greenhouse` (CoAPS actuators) |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED` | CoAPS demos using PSK |

Plain CoAP on port **5683** works with default OpenThread settings. CoAPS on
**5684** requires the secure API and mbedtls PSK cipher suite — see
[CoAP Secure — Enabling CoAPS](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md) and
[CoAP Greenhouse — Enabling CoAPS](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md) for **Enabling CoAPS
(Arduino as IDF component)** using `idf.py menuconfig`.

If `OThreadCoAP::secureApiEnabled()` is false at runtime, rebuild the sketch as
an [ESP-IDF project with Arduino as a component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
and enable the OpenThread, mbedtls PSK, and custom-header CoAPS options described
in those group READMEs.

## Advanced server / client options

Most demos use default port **5683** and the library CoAP retransmit profile.
These optional calls are documented in
[OpenThread CoAP API documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html)
but not shown in every sketch:

```cpp
// Custom listen port (call before begin(); server must start first on shared UDP service)
OThreadCoAPServer.setPort(5683);
OThreadCoAPServer.begin();

// RFC 7252 default retransmit parameters before a short client timeout
OThreadCoAPClient client;
client.useDefaultCoapRetransmit();
client.setTimeout(3000);

// Leave a multicast group before stop() when toggling membership at runtime
OThreadCoAPServer.leaveMulticastGroup(IPAddress("ff03::abcd"));
```

See [CoAP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) for
`joinMulticastGroup()` in a full multicast server.

## Troubleshooting

**Startup order:** For multi-board CoAP demos, **start the server / Leader sketch
first**, wait for attached (+ `Commissioner ready` when commissioning), then flash
or **reset** the client.

| Symptom | Likely cause |
| --- | --- |
| Client/Joiner cannot attach | **Start the server / Leader sketch first.** Wait for attached (+ `Commissioner ready` when commissioning). Then reset or flash the client. |
| Client shows attached but CoAP fails | Server reset or not listening — client may still target an old Leader RLOC or network. Reset the client after the server is fully up. |
| CoAPS / DTLS errors | Build flags missing — see **Enabling CoAPS** in [CoAP Secure — Enabling CoAPS](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md) or [CoAP Greenhouse — Enabling CoAPS](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). Plain CoAP on 5683 works without them. |
| Works once, fails after reboot | Check whether the demo uses `initNew()` (new network each boot) vs NVS resume — client may need a reset to re-join. See the specific demo README. |

## See Also

* [Native examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md) — Native API overview and all example categories.
* [Native UDP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP) — same IoT patterns on application ports 5050/5051.
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — Joiner / Commissioner without CoAP.
* [CLI CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) — same scenarios driven by OpenThread CLI commands.

## License

Apache License 2.0.
