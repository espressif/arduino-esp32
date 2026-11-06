# greenhouse_server - Thread Leader + Commissioner + Dual CoAP Gateway

Server side of the [CoAP Greenhouse demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). This sketch:

* forms a Thread network `ESP_OT_CoAP_Greenhouse` with a hard-coded `DataSet`,
* petitions the **Commissioner** role and opens a joiner window for PSKd
  `J01NME`,
* runs a **plain CoAP server** on port **5683** for read-only telemetry
  (`greenhouse/temp`, `greenhouse/light`),
* runs a **CoAPS server** on port **5684** for authenticated actuator commands
  (`valve/water`, `fan/speed`),
* simulates drifting temperature and light levels in `loop()`.

It is the counterpart of [CoAP Greenhouse — greenhouse_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client).

**Build:** CoAPS requires `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1`. If
`OThreadCoAP::secureApiEnabled()` is false, rebuild as an
[ESP-IDF project with Arduino as a component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
and enable the CoAPS options in menuconfig — see **Enabling CoAPS** in
[CoAP Greenhouse — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). Plain telemetry on 5683 works without CoAPS.

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
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | CoAPS actuator server on 5684.          |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED`   | PSK cipher suite for the demo.                   |

## Prerequisites

Flash this sketch **before** [CoAP Greenhouse — greenhouse_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client).

Wait for `Commissioner ready (PSKd "J01NME")`, plain CoAP on 5683, and CoAPS
on 5684 before flashing or resetting the client.

After any server reset, reset the client so it re-joins.

## What the sketch does

```cpp
// 1) Form network and petition Commissioner.
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
OThread.startCommissioner();
OThread.addJoiner("J01NME", JOINER_WINDOW_SEC);

// 2) Plain CoAP telemetry (5683).
OThreadCoAPServer.on("greenhouse/temp",  OT_COAP_METHOD_GET, onGreenhouseTemp);
OThreadCoAPServer.on("greenhouse/light", OT_COAP_METHOD_GET, onGreenhouseLight);
OThreadCoAPServer.begin();

// 3) CoAPS actuators (5684).
OThreadCoAPSecureServer.setPSK(COAP_PSK, sizeof(COAP_PSK), "esp-coap-demo");
OThreadCoAPSecureServer.on("valve/water", OT_COAP_METHOD_PUT, onValveWater);
OThreadCoAPSecureServer.on("fan/speed",   OT_COAP_METHOD_PUT, onFanSpeed);
OThreadCoAPSecureServer.begin();

// 4) loop(): simulate temp/light drift; fan cools greenhouse
```

## Resources

| Path | Transport | Methods | Response |
| --- | --- | --- | --- |
| `greenhouse/temp` | Plain 5683 | GET | Temperature string (°C) |
| `greenhouse/light` | Plain 5683 | GET | Light level (lux) |
| `valve/water` | CoAPS 5684 | PUT | Body 0–100 → `204 Changed` |
| `fan/speed` | CoAPS 5684 | PUT | Body 0–100 → `204 Changed` |

Handlers log `[CON]` or `[NON]` and the remote IP on each request.

## Expected serial output

```text
=== CoAP Greenhouse — server ===
Forming Thread network...
Waiting for attach..
Attached as Leader.
Starting Commissioner...
Commissioner ready (PSKd "J01NME")
Starting CoAP servers...
Ready.
Plain CoAP on port 5683: GET greenhouse/temp, greenhouse/light
CoAPS on port 5684: PUT valve/water, fan/speed (PSK id "esp-coap-demo")
Mesh-local: fdde:ad00:beef:0:....
[NON] GET greenhouse/temp from fdde:ad00:beef:0:....
[CoAPS CON] PUT fan/speed from fdde:ad00:beef:0:....
Fan -> 75%
Valve -> 60%
```

If CoAPS is not in the platform build (plain telemetry still works):

```text
=== CoAP Greenhouse — server ===
...
Ready.
Plain CoAP on port 5683: GET greenhouse/temp, greenhouse/light
CoAPS is not enabled in this build. Secure actuators will not run.
Plain CoAP telemetry continues.
Mesh-local: fdde:ad00:beef:0:....
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Joiner secret accepted by the Commissioner.                  |
| `JOINER_WINDOW_SEC`| How long `addJoiner()` stays valid (default 600 s).          |
| `CHANNEL`          | 802.15.4 channel (default 15).                               |
| `PAN_ID`           | 16-bit PAN ID (default `0xBEE5`).                            |
| `NETKEY`           | 128-bit network key.                                         |
| `COAP_PSK`         | 16-byte CoAPS pre-shared key — must match client.            |
| `COAP_PSK_ID`      | PSK identity string (default `esp-coap-demo`).               |

Plain telemetry uses port **5683**; actuators use CoAPS port **5684**.

## Troubleshooting

**Startup order:** Flash this **greenhouse_server** first. Wait for Commissioner
ready and both servers listening, then flash
[CoAP Greenhouse — greenhouse_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client).

| Symptom | Likely cause |
| --- | --- |
| `CoAPS is not enabled in this build` | CoAPS is missing from the firmware. Rebuild using [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) — see **Enabling CoAPS** in [CoAP Greenhouse — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). Server continues with plain telemetry only. |
| `CoAPS server start failed` | CoAPS enabled in build but `OThreadCoAPSecureServer.begin()` failed (PSK, port, or attach). |
| `Plain CoAP server start failed` | OpenThread not attached. |
| Client join fails | Commissioner window closed — reset server or extend `JOINER_WINDOW_SEC`. |
| Client plain GET works, CoAPS PUT fails | PSK mismatch or CoAPS not built on client. |
| Client worked, then stops after server reset | Reset client to re-join. |

## See also

* [CoAP Greenhouse — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md) — resource table and dual-transport architecture.
* [CoAP Greenhouse — greenhouse_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client) — Joiner + automation client.
* [CoAP Secure — secure_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server) — CoAPS-only server.

## License

Apache License 2.0.
