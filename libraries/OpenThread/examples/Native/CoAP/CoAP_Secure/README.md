# CoAP Secure (CoAPS) — Native API

Demonstrates **CoAP over DTLS** using `OThreadCoAPSecureServer` and
`OThreadCoAPSecureClient` with a shared **Pre-Shared Key (PSK)** — aligned with
OpenThread CLI `coaps psk <key> <id>`.

| Sketch | Role |
| --- | --- |
| [secure_server (Leader + CoAPS server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server) | Thread Leader + Commissioner + CoAPS server on port **5684** |
| [secure_client (Joiner + CoAPS client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client) | Joiner + DTLS connect + GET `status` |

The client joins via Commissioner (PSKd `J01NME`), opens a DTLS session to the
Leader RLOC, performs one confirmable GET `status`, prints the payload, and
disconnects.

## Compared to plain CoAP

| Plain `OThreadCoAPClient` | `OThreadCoAPSecureClient` |
| --- | --- |
| `GET(host, "path")` every call | `connect(host)` once, then `GET("path")` |
| UDP port 5683 | DTLS port 5684 |
| No credentials | `setPSK()` before connect |

## How to Run

1. Build with CoAPS enabled (see **Required IDF features** below).
2. Flash [secure_server (Leader + CoAPS server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server)
   first and wait for `Commissioner ready` and CoAPS listening on 5684.
3. Flash [secure_client (Joiner + CoAPS client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client)
   on a second board.
4. Watch the client Serial for `DTLS connected` and `GET status` output.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Purpose |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | OpenThread stack |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | 802.15.4 radio |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `secure_server` |
| `CONFIG_OPENTHREAD_JOINER=y` | `secure_client` |
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | CoAPS / DTLS API |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED` | PSK cipher suite for demo |

Without the CoAPS build flags the sketches compile but secure operations return
`OT_COAP_ERROR_NOT_CONNECTED` at runtime. Use `OThreadCoAP::secureApiEnabled()`
at runtime to check whether CoAPS is present in your firmware.

## Enabling CoAPS (Arduino as IDF component)

The standard Arduino IDE build may not include the CoAPS secure API. If you need
CoAPS, build the sketch as an **ESP-IDF project with Arduino as a component**,
then enable the required features in **menuconfig**.

See the [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
guide. Copy the sketch into the project `main/` folder (rename `.ino` to `.cpp`),
run `idf.py set-target <soc>`, then `idf.py menuconfig`.

Enable these options:

| menuconfig | Setting |
| --- | --- |
| Component config → OpenThread | `OpenThread` |
| Component config → OpenThread → Thread Core Features | `Enable Commissioner` (`secure_server`) |
| Component config → OpenThread → Thread Core Features | `Enable Joiner` (`secure_client`) |
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

Shared demo credentials (both sketches):

| Parameter | Value |
| --- | --- |
| Thread Joiner PSKd | `J01NME` |
| CoAPS PSK id | `esp-coap-demo` |
| CoAPS PSK bytes | 16-byte array in both `.ino` files (must match) |

## Troubleshooting

**Startup order:** Flash [CoAP Secure — secure_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server) first and wait for
`Commissioner ready` and CoAPS listening on 5684. Then flash
[CoAP Secure — secure_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client). Reset the client if it booted before
Commissioner was active.

| Symptom | Likely cause |
| --- | --- |
| `CoAPS is not enabled in this build` | CoAPS is missing from the firmware. Rebuild using [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) and enable the options in **Enabling CoAPS** above. |
| Client `Join failed` | **Flash `secure_server` first** and wait for `Commissioner ready`. Reset client after server is up. |
| `DTLS connect failed` | CoAPS not enabled in build, or `COAP_PSK` / PSK id mismatch between server and client. |
| Client attached but GET fails | Server not listening on 5684, or secure server failed to start (check build flags). |
| Server reset after client joined | Client may be on old network — reset client to re-join through Commissioner. |

## See Also

* [CoAP Greenhouse](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse) — plain + CoAPS combined on one gateway.
* [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md) — all Native CoAP demos and port conventions.

## License

Apache License 2.0.
