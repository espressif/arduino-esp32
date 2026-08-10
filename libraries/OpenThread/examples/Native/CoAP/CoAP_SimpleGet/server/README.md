# server - Thread Leader + CoAP Server

Server side of the [CoAP Simple GET demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/README.md). This sketch:

* forms a small Thread network `ESP_OT_CoAP_Demo` with a hard-coded `DataSet`,
* becomes the **Leader** and prints the mesh-local EID,
* starts a plain **CoAP server** on port **5683**,
* responds to GET `hello` with `"Hello from CoAP!"` in callback mode (no CoAP
  polling in `loop()`).

It is the counterpart of [CoAP SimpleGet — client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client). Flash this sketch
**before** the client on a second board in RF range.

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

Flash this sketch **before** [CoAP SimpleGet — client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client). The client only needs the
same `NETKEY`; channel, PAN ID, and Extended PAN ID are defined here on the Leader.

## What the sketch does

```cpp
// 1) Form the Thread network and wait for Leader attach.
DataSet ds;
ds.initNew();
ds.setNetworkName("ESP_OT_CoAP_Demo");
ds.setChannel(CHANNEL);
ds.setPanId(PAN_ID);
ds.setExtendedPanId(EXTPANID);
ds.setNetworkKey(NETKEY);
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();

// 2) Register GET handler and start CoAP.
OThreadCoAPServer.on("hello", OT_COAP_METHOD_GET, onHello);
OThreadCoAPServer.begin();
// onHello() sends OT_COAP_RESP_OK + "Hello from CoAP!"
```

## Expected serial output

```text
=== CoAP Simple GET — server ===
Forming Thread network...
Waiting for attach..
Attached as Leader.
Mesh-local EID: fdde:ad00:beef:0:....
Leader RLOC: fdde:ad00:beef:0:0:ff:fe00:0
Starting CoAP server...
Ready. CoAP server listening on path 'hello'
```

If attach fails:

```text
=== CoAP Simple GET — server ===
Forming Thread network...
Waiting for attach.........
Attach timeout.
Thread attach failed.
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant   | Purpose                                                      |
| ---------- | ------------------------------------------------------------ |
| `CHANNEL`  | 802.15.4 channel (default 24).                               |
| `PAN_ID`   | 16-bit PAN ID (default `0xC0A0`).                            |
| `EXTPANID` | 64-bit Extended PAN ID — fixed; must not change between runs. |
| `NETKEY`   | 128-bit network key — must match the client.                 |

CoAP listens on the standard port **5683** (`OThreadCoAPServer.begin()`).

## Troubleshooting

**Startup order:** Flash this **server (Leader)** sketch first. Wait for
`Attached as Leader`, then flash [CoAP SimpleGet — client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client).

| Symptom | Likely cause |
| --- | --- |
| `Thread attach failed` | RF issue or conflicting NVS dataset — erase flash and retry. |
| `CoAP server start failed` | OpenThread not attached or port conflict. |
| Client GET times out | Server not running, or client formed its own network (check for `Started as Leader` on client). |

## See also

* [CoAP SimpleGet — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/README.md) — shared dataset table and bring-up steps.
* [CoAP SimpleGet — client](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client) — one-shot GET client sketch.
* [OpenThread CoAP API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html)

## License

Apache License 2.0.
