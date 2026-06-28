# greenhouse_client - Thread Joiner + Dual CoAP Controller

Client side of the [CoAP Greenhouse demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). This sketch:

* runs the **Thread Joiner** state machine in `setup()` using PSKd `J01NME`,
  then calls `start()` after successful commissioning,
* polls **plain CoAP** telemetry every **8 s** with **NON** GET on
  `greenhouse/temp` and `greenhouse/light`,
* runs a simple automation loop every **24 s** that sends **CoAPS CON PUT**
  to `fan/speed` and `valve/water` based on last readings,
* opens a DTLS session per control cycle (`connect` → PUT → `disconnect`).

It is the counterpart of [CoAP Greenhouse — greenhouse_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server).

**Build:** CoAPS requires `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1`. If
`OThreadCoAP::secureApiEnabled()` is false, rebuild as an
[ESP-IDF project with Arduino as a component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
and enable the CoAPS options in menuconfig — see **Enabling CoAPS** in
[CoAP Greenhouse — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). Plain telemetry polling still works without CoAPS.

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
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable `otJoiner*` APIs used by the sketch.      |
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | CoAPS actuator client on 5684.          |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED`   | PSK cipher suite for the demo.                   |

## Prerequisites

A [CoAP Greenhouse — greenhouse_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server) instance must already be running
with Commissioner ready and both plain (5683) and CoAPS (5684) servers listening.

The client joins **only in `setup()`** (retries with `stop()` between attempts).
Reset the client if it booted before the server Commissioner was active.

`COAP_PSK` and `COAP_PSK_ID` must match the server. Control commands are skipped
until the first successful telemetry poll sets `lastTempC > 0`.

## What the sketch does

```cpp
// 1) Join the greenhouse network via Commissioner.
OThread.startJoiner("J01NME", JOIN_TIMEOUT_MS);
OThread.start();
serverIp = OThread.getLeaderRloc();

// 2) Every TELEMETRY_MS: NON GET telemetry.
PlainClient.setConfirmable(false);
PlainClient.GET(serverIp, "greenhouse/temp");
PlainClient.GET(serverIp, "greenhouse/light");

// 3) Every CONTROL_MS: automation + CoAPS CON PUT.
SecureClient.connect(serverIp);
SecureClient.PUT("fan/speed",   fanPercent);    // temp > 26 C -> 75%
SecureClient.PUT("valve/water", valvePercent);  // light < 8000 lux -> 60%
SecureClient.disconnect();
```

Default automation thresholds: `TEMP_FAN_ON_C = 26.0`, `LIGHT_VALVE_LUX = 8000`.

## Expected serial output

```text
=== CoAP Greenhouse — client ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Waiting for attach..
Attached as Child. Greenhouse server: fdde:ad00:beef:0:0:ff:fe00:0
Ready. Polling telemetry and running control loop.
[plain NON] temp=24.3 C
[plain NON] light=11500 lux
Control: temp=24.3 fan=15% light=11500 valve=10%
[CoAPS CON] PUT fan/speed=15 -> 2.04 Changed
[CoAPS CON] PUT valve/water=10 -> 2.04 Changed
```

If CoAPS is not in the platform build (plain telemetry still works):

```text
=== CoAP Greenhouse — client ===
...
CoAPS is not enabled in this build. Secure control will not run.
Ready. Polling telemetry and running control loop.
[plain NON] temp=24.3 C
[plain NON] light=11500 lux
```

If join fails:

```text
=== CoAP Greenhouse — client ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Joiner failed: 7
Join failed, retry in 3s...
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Pre-Shared Key for Device. Must match the server Commissioner. |
| `CHANNEL_HINT`     | 802.15.4 channel hint (default 15).                          |
| `JOIN_TIMEOUT_MS`  | Max wait inside `startJoiner()` for the commissioner.        |
| `TELEMETRY_MS`     | Plain NON poll interval (default 8000 ms).                   |
| `CONTROL_MS`       | CoAPS control loop interval (default 24000 ms).              |
| `TEMP_FAN_ON_C`    | Fan speed 75% when temp exceeds this (default 26.0).         |
| `LIGHT_VALVE_LUX`  | Valve 60% when light below this (default 8000).              |
| `COAP_PSK`         | 16-byte CoAPS pre-shared key — must match server.            |
| `COAP_PSK_ID`      | PSK identity string (default `esp-coap-demo`).               |

Plain client timeout: `PlainClient.setTimeout(3000)`. Secure connect timeout:
`SecureClient.setConnectTimeout(10000)`.

## Troubleshooting

**Startup order:** Start [CoAP Greenhouse — greenhouse_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server) first and
wait for Commissioner ready and both servers listening. Reset this client if
it booted before the server was ready.

| Symptom | Likely cause |
| --- | --- |
| `Join failed, retry in 3s...` | Server Commissioner not ready — start server first, then reset client. |
| `CoAPS is not enabled in this build` | CoAPS is missing from the firmware. Rebuild using [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) — see **Enabling CoAPS** in [CoAP Greenhouse — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md). Client skips secure control; plain GET still runs. |
| Plain GET works, `DTLS connect failed` | Server CoAPS not running, PSK mismatch, or CoAPS not enabled in the build. |
| No control output for ~24 s | Control waits for first successful telemetry (`lastTempC > 0`). |
| `CON PUT ... failed: Timeout` | Server CoAPS not running or detached from mesh. |
| Worked once, fails after server reset | Reset client to re-join. |

## See also

* [CoAP Greenhouse — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/README.md) — resource table and dual-transport architecture.
* [CoAP Greenhouse — greenhouse_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_server) — companion dual-transport gateway.
* [CoAP Secure — secure_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_client) — CoAPS-only client.

## License

Apache License 2.0.
