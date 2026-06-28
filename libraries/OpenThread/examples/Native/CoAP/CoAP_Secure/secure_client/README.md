# secure_client - Thread Joiner + CoAPS Client

Client side of the [CoAP Secure demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md). This sketch:

* runs the **Thread Joiner** state machine in `setup()` using PSKd `J01NME`
  and a channel hint, then calls `start()` after successful commissioning,
* resolves the secure server as the Thread **Leader RLOC**,
* opens a **DTLS** session with the shared CoAPS PSK (`esp-coap-demo`),
* performs one **confirmable** GET on path `status`, prints the payload, and
  disconnects.

It is the counterpart of [CoAP Secure — secure_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server).

**Build:** requires `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1`. If
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
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable `otJoiner*` APIs used by the sketch.      |
| `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | CoAPS / DTLS client API.                |
| `MBEDTLS_KEY_EXCHANGE_PSK_ENABLED`   | PSK cipher suite for the demo.                   |

## Prerequisites

A [CoAP Secure — secure_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server) instance must already be running with
`Commissioner ready (PSKd "J01NME")` and CoAPS listening on port **5684**.

The client joins **only in `setup()`** (retries with `stop()` between attempts).
Reset the client if it booted before the server Commissioner was active.

`COAP_PSK` and `COAP_PSK_ID` must match the server exactly.

## What the sketch does

```cpp
// 1) Join the secure_server network via Commissioner.
OThread.setChannel(CHANNEL_HINT);
OThread.networkInterfaceUp();
OThread.startJoiner("J01NME", JOIN_TIMEOUT_MS);
OThread.start();
serverIp = OThread.getLeaderRloc();

// 2) Open DTLS, GET status, disconnect.
SecureClient.setPSK(COAP_PSK, sizeof(COAP_PSK), "esp-coap-demo");
SecureClient.setConfirmable(true);
SecureClient.connect(serverIp);
int code = SecureClient.GET("status");
SecureClient.disconnect();
```

## Expected serial output

```text
=== CoAP Secure — client ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Waiting for attach..
Attached as Child. Server: fdde:ad00:beef:0:0:ff:fe00:0
Connecting DTLS...
DTLS connected
GET status -> 205 (2.05 Content)
Payload: secure-ok
DTLS closed by peer
```

If CoAPS is not in the platform build:

```text
=== CoAP Secure — client ===
CoAPS is not enabled in this build. This sketch will not run.
```

If join or DTLS fails (secure API enabled):

```text
=== CoAP Secure — client ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Joiner failed: 7
Join failed, retry in 3s...
```

or

```text
Attached. Server: fdde:ad00:beef:0:0:ff:fe00:0
DTLS connect failed.
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Pre-Shared Key for Device. Must match the server Commissioner. |
| `CHANNEL_HINT`     | 802.15.4 channel hint (default 15). Must match the network.  |
| `JOIN_TIMEOUT_MS`  | Max wait inside `startJoiner()` for the commissioner.        |
| `COAP_PSK`         | 16-byte CoAPS pre-shared key — must match server.            |
| `COAP_PSK_ID`      | PSK identity string (default `esp-coap-demo`).               |

Timeouts: `setConnectTimeout(10000)` for DTLS handshake,
`setTimeout(5000)` for CoAP request/response.

## Troubleshooting

**Startup order:** Start [CoAP Secure — secure_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server) first and wait for
Commissioner ready and CoAPS listening. Reset this client if it booted before
the server was ready.

| Symptom | Likely cause |
| --- | --- |
| `CoAPS is not enabled in this build` | CoAPS is missing from the firmware. Rebuild using [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) — see **Enabling CoAPS** in [CoAP Secure — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md). |
| `Join failed, retry in 3s...` | Server Commissioner not ready — start server first, then reset client. |
| `DTLS connect failed` | CoAPS build flags missing, or PSK / PSK id mismatch with server. |
| GET fails after attach | Server not listening on 5684. |
| Worked once, fails after server reset | Server `initNew()` each boot — **reset client** to re-join. |

## See also

* [CoAP Secure — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/README.md) — build requirements and Enabling CoAPS.
* [CoAP Secure — secure_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure/secure_server) — companion CoAPS server sketch.
* [CoAP Greenhouse — greenhouse_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client) — plain + CoAPS combined client.

## License

Apache License 2.0.
