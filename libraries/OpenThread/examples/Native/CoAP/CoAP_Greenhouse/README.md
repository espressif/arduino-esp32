# CoAP Greenhouse — Dual Transport over Thread (Native API)

Typical IoT split: **open telemetry** on plain CoAP, **authenticated actuators**
on CoAPS. One server and one client sketch demonstrate multiple resources,
CON vs NON, and both transports on the same mesh node pair.

| Sketch | Role |
| --- | --- |
| [greenhouse_server (Leader + gateway)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server) | Leader + Commissioner + plain + CoAPS servers |
| [greenhouse_client (Joiner + controller)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client) | Joiner + plain NON polls + CoAPS CON commands |

```
                         ┌─────────────────────────────────────┐
  greenhouse_client      │  PlainClient (5683)                 │
                         │    NON GET greenhouse/temp          │
                         │    NON GET greenhouse/light         │
                         │  SecureClient (5684)                │
                         │    CON PUT valve/water              │
                         │    CON PUT fan/speed                │
                         └─────────────────┬───────────────────┘
                                           │ Thread mesh
                    ┌──────────────────────▼──────────────────────────┐
  greenhouse_server │  OThreadCoAPServer        — telemetry (read)    │
                    │  OThreadCoAPSecureServer  — actuators (write)   │
                    └─────────────────────────────────────────────────┘
```

## Resources

| Path | Transport | Methods | Client reliability | Purpose |
| --- | --- | --- | --- | --- |
| `greenhouse/temp` | Plain 5683 | GET | **NON** | Temperature (°C) |
| `greenhouse/light` | Plain 5683 | GET | **NON** | Light level (lux) |
| `valve/water` | CoAPS 5684 | PUT | **CON** | Irrigation 0–100 % |
| `fan/speed` | CoAPS 5684 | PUT | **CON** | Ventilation 0–100 % |

## How to Run

1. Enable CoAPS build flags (see **Required IDF features** below).
2. Flash [greenhouse_server (Leader + gateway)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server)
   first and wait for Commissioner ready and both plain/CoAPS listen messages.
3. Flash [greenhouse_client (Joiner + controller)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client)
   on a second board.
4. Watch Serial: client polls telemetry every 8 s and sends CON actuator
   commands every 24 s (after first successful telemetry read).

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Purpose |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | OpenThread stack |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | 802.15.4 radio |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `greenhouse_server` |
| `CONFIG_OPENTHREAD_JOINER=y` | `greenhouse_client` |
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | CoAPS actuators on 5684 |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED` | PSK cipher suite |

Shared demo credentials:

| Parameter | Value |
| --- | --- |
| Network name | `ESP_OT_CoAP_Greenhouse` |
| Channel | 15 |
| PAN ID | `0xBEE5` |
| Thread Joiner PSKd | `J01NME` |
| CoAPS PSK id | `esp-coap-demo` |

Plain CoAP telemetry on **5683** works without CoAPS build flags, but actuator
PUT commands require them. Use `OThreadCoAP::secureApiEnabled()` at runtime to
check whether CoAPS is present in your firmware.

## Enabling CoAPS (Arduino as IDF component)

The standard Arduino IDE build may not include the CoAPS secure API. If you need
CoAPS actuators on port **5684**, build the sketch as an **ESP-IDF project with
Arduino as a component**, then enable the required features in **menuconfig**.

See the [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
guide. Copy the sketch into the project `main/` folder (rename `.ino` to `.cpp`),
run `idf.py set-target <soc>`, then `idf.py menuconfig`.

Enable these options:

| menuconfig | Setting |
| --- | --- |
| Component config → OpenThread | `OpenThread` |
| Component config → OpenThread → Thread Core Features | `Enable Commissioner` (`greenhouse_server`) |
| Component config → OpenThread → Thread Core Features | `Enable Joiner` (`greenhouse_client`) |
| Component config → mbedTLS → TLS Key Exchange Methods | `Enable pre-shared-key ciphersuites` |
| Component config → mbedTLS → TLS Key Exchange Methods | `Enable PSK based ciphersuite modes` |
| Component config → OpenThread → Thread Extensioned Features | `Use a header file defined by customer` |

The CoAPS API flag `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE` is not a direct
menuconfig toggle. When using a custom OpenThread header, define:

```c
#define OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE 1
```

Set the header path under **Thread Extensioned Features → OpenThread Custom Header
Config**, save, then build with `idf.py build flash monitor`.

## Troubleshooting

**Startup order:** Flash [greenhouse_server (Leader + gateway)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server) first and
wait for `Commissioner ready` and both plain and CoAPS listen messages. Then
flash [greenhouse_client (Joiner + controller)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client). Reset the client if it booted
before Commissioner was active.

| Symptom | Likely cause |
| --- | --- |
| Client `Join failed` | **Flash `greenhouse_server` first** and wait for Commissioner ready. Reset client after server is ready. |
| `CoAPS is not enabled in this build` | CoAPS is missing from the firmware. Rebuild using [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) and enable the options in **Enabling CoAPS** above. Plain telemetry on 5683 still works. |
| Plain GET works, CoAPS PUT fails | CoAPS build flags or PSK mismatch — telemetry on 5683 can work without secure build. |
| Control skipped on client | Wait for first successful NON telemetry poll (`lastTempC` must be non-zero). |
| Server reset after client joined | Reset client to re-join; in-memory server state is lost on reboot. |

## See Also

* [CoAP Secure](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure) — CoAPS-only minimal demo.
* [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md) — all Native CoAP demos and port conventions.

## License

Apache License 2.0.
