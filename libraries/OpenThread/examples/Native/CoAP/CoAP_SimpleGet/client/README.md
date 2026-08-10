# client - Thread Child + CoAP GET Client

Client side of the [CoAP Simple GET demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/README.md). This sketch:

* joins the Leader started by [CoAP SimpleGet — server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server) using only the shared
  **network key** (RouterNode pattern — no `initNew()` on the client),
* resolves the **Leader RLOC** as its CoAP destination,
* performs a single **confirmable** GET on path `hello` in `setup()`,
* prints the CoAP response code and payload, then idles in `loop()`.

It is the counterpart of [CoAP SimpleGet — server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server). This is a
one-shot demo — it does not repeat the GET after boot.

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

## Prerequisites

The client only needs the same `NETKEY` as the server. Flash the server first
and wait for `Ready. CoAP server listening on path 'hello'`, then flash this client.
Reset the client if it booted before the server was Leader.

## What the sketch does

```cpp
// 1) Join using the network key only (channel/PAN/ExtPAN learned at attach).
DataSet ds;
ds.clear();
ds.setNetworkKey(NETKEY);
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
serverIp = OThread.getLeaderRloc();

// 2) One confirmable GET in setup().
CoapClient.setTimeout(3000);
CoapClient.setConfirmable(true);
int code = CoapClient.GET(serverIp, "hello");
Serial.printf("Payload: %s\n", CoapClient.getString().c_str());
```

## Expected serial output

```text
=== CoAP Simple GET — client ===
Joining Thread network...
Waiting for attach..
Attached as Child. Server (Leader RLOC): fdde:ad00:beef:0:0:ff:fe00:0
Ready. Sending GET hello...
GET hello -> code 205 (2.05 Content)
Payload: Hello from CoAP!
```

If the server is not running first:

```text
=== CoAP Simple GET — client ===
Joining Thread network...
Waiting for attach..
Started as Leader — is server.ino running first?
Attach timeout.
Thread attach failed.
```

If attach succeeds but GET fails:

```text
Ready. Sending GET hello...
GET hello -> code -11 (unknown)
Error: timeout
```

(`OT_COAP_ERROR_TIMEOUT` is **-11**; the second line uses `OThreadCoAP::errorToString(code)`.)

## Customization

All tunables are at the top of the `.ino` file:

| Constant  | Purpose                                      |
| --------- | -------------------------------------------- |
| `NETKEY`  | 128-bit network key — must match the server. |

CoAP client timeout is set in `setup()` via `CoapClient.setTimeout(3000)` (ms).

## Troubleshooting

**Startup order:** Start [CoAP SimpleGet — server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server) first and wait for Leader attach
and CoAP listening. Then flash or reset this client.

| Symptom | Likely cause |
| --- | --- |
| `Thread attach failed` | Server not Leader yet, or wrong `NETKEY`. |
| `Started as Leader` | Server not running — client formed its own network. |
| `GET hello -> error timeout` | Client on a separate partition — start server first, erase client flash, retry. Check Serial for `code -11` and `Error: timeout`. |
| Wrong payload | Connected to a different network — erase NVS on both boards. |

## See also

* [CoAP SimpleGet — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/README.md) — shared dataset table and bring-up steps.
* [CoAP SimpleGet — server](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/server) — companion Leader + CoAP server sketch.
* [OpenThread CoAP API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html)

## License

Apache License 2.0.
