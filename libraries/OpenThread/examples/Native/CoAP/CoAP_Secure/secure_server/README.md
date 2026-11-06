# secure_server - Thread Leader + Commissioner + CoAPS Server

Server side of the [CoAP Secure demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md). This sketch:

* forms a Thread network `ESP_OT_CoAP_Secure` with a hard-coded `DataSet`
  (`initNew()` on each boot),
* petitions the **Commissioner** role and opens a joiner window for PSKd
  `J01NME`,
* starts a **CoAPS server** on port **5684** with PSK authentication,
* serves GET `status` with payload `"secure-ok"` in callback mode.

It is the counterpart of [CoAP Secure — secure_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client).

**Build:** CoAPS requires `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1`. If
`OThreadCoAP::secureApiEnabled()` is false, rebuild as an
[ESP-IDF project with Arduino as a component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
and enable the CoAPS options in menuconfig — see **Enabling CoAPS** in
[CoAP Secure — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md).

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF features (sdkconfig)

| Feature                              | Why                                              |
| ------------------------------------ | ------------------------------------------------ |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build the OpenThread stack.                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure the SoC has the 802.15.4 radio.           |
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable Commissioner APIs for Joiner admission.   |
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | CoAPS / DTLS server API.                |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED`   | PSK cipher suite for the demo.                   |

## Prerequisites

Flash this sketch **before** [CoAP Secure — secure_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client).

Wait for `Commissioner ready (PSKd "J01NME")` and
`CoAPS listening on port 5684` before flashing or resetting the client.

After any server reset, reset the client so it re-joins the new network.

## What the sketch does

```cpp
// 1) Form the Thread network and petition Commissioner.
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
OThread.startCommissioner();
OThread.addJoiner("J01NME", JOINER_WINDOW_SEC);

// 2) Configure PSK and start CoAPS server.
OThreadCoAPSecureServer.setPSK(COAP_PSK, sizeof(COAP_PSK), "esp-coap-demo");
OThreadCoAPSecureServer.on("status", OT_COAP_METHOD_GET, onStatus);
OThreadCoAPSecureServer.begin();
// onStatus() returns OT_COAP_RESP_OK + "secure-ok"
```

## Expected serial output

```text
=== CoAP Secure — server ===
Forming Thread network...
Waiting for attach..
Attached as Leader.
Starting Commissioner...
Commissioner ready (PSKd "J01NME")
Starting CoAPS server...
Ready. CoAPS listening on port 5684 (PSK id "esp-coap-demo")
Mesh-local: fdde:ad00:beef:0:....
CoAPS GET from fdde:ad00:beef:0:....
```

If CoAPS is not in the platform build:

```text
=== CoAP Secure — server ===
CoAPS is not enabled in this build. This sketch will not run.
```

If CoAPS fails to start (secure API enabled):

```text
=== CoAP Secure — server ===
CoAPS server start failed.
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Joiner secret accepted by the Commissioner.                  |
| `JOINER_WINDOW_SEC`| How long `addJoiner()` stays valid (default 600 s).          |
| `CHANNEL`          | 802.15.4 channel (default 15).                               |
| `PAN_ID`           | 16-bit PAN ID (default `0x5EC0`).                            |
| `NETKEY`           | 128-bit network key.                                         |
| `COAP_PSK`         | 16-byte CoAPS pre-shared key — must match client.            |
| `COAP_PSK_ID`      | PSK identity string (default `esp-coap-demo`).               |

CoAPS listens on port **5684** (`OT_COAP_SECURE_DEFAULT_PORT`).

## Troubleshooting

**Startup order:** Flash this **secure_server** first. Wait for Commissioner
ready and CoAPS listening, then flash [CoAP Secure — secure_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client).

| Symptom | Likely cause |
| --- | --- |
| `CoAPS is not enabled in this build` | CoAPS is missing from the firmware. Rebuild using [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) — see **Enabling CoAPS** in [CoAP Secure — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md). |
| `CoAPS server start failed` | CoAPS build flags not enabled. |
| Client join fails | Commissioner window closed — reset server or extend `JOINER_WINDOW_SEC`. |
| Client `DTLS connect failed` | `COAP_PSK` or PSK id mismatch with client. |
| Client worked, then stops after server reset | Server `initNew()` each boot — **reset client** to re-join. |

## See also

* [CoAP Secure — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md) — build requirements and Enabling CoAPS.
* [CoAP Secure — secure_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client) — Joiner + DTLS GET client.
* [CoAP Greenhouse — greenhouse_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server) — plain + CoAPS dual server.

## License

Apache License 2.0.
