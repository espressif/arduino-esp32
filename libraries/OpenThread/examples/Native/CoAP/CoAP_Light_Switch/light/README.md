# light - Thread Leader + Commissioner + CoAP Lamp Server

Server side of the [CoAP Light Switch demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/README.md). This sketch:

* forms a Thread network `ESP_OT_CoAP_Lamp` with a hard-coded `DataSet`,
* petitions the **Commissioner** role and opens a joiner window for PSKd
  `J01NME`,
* starts a plain **CoAP server** on port **5683** with resource path `Lamp`,
* subscribes to realm-local multicast group **`ff03::abcd`** so it receives
  switch PUT commands,
* drives an on-board **RGB LED** lamp (white on/off with fade animation in
  `loop()`); CoAP handlers only set `lampTarget` (no `delay()` on OT task).

It is the counterpart of [CoAP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch). One light can
serve many switches on the same network.

## Supported Targets

| SoC      | Thread | RGB LED  | Status    |
| -------- | ------ | -------- | --------- |
| ESP32-H2 | yes    | Required | Supported |
| ESP32-C6 | yes    | Required | Supported |
| ESP32-C5 | yes    | Required | Supported |

## Required IDF features (sdkconfig)

| Feature                              | Why                                              |
| ------------------------------------ | ------------------------------------------------ |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build the OpenThread stack.                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure the SoC has the 802.15.4 radio.           |
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable Commissioner APIs for Joiner admission.   |

## Prerequisites

Flash this sketch **before** any [CoAP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch) boards.

The server calls `ds.initNew()` on every boot. After any server reset, **reset
all switches** so they re-join through the Commissioner.

Wait for `Commissioner ready (PSKd "J01NME")` before flashing or resetting
switches.

## What the sketch does

```cpp
// 1) Form the Thread network and petition Commissioner.
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
OThread.startCommissioner();
OThread.addJoiner("J01NME", JOINER_WINDOW_SEC);

// 2) Start CoAP, register Lamp handler, join multicast group.
OThreadCoAPServer.on("Lamp", OT_COAP_METHOD_GET | OT_COAP_METHOD_PUT, onLamp);
OThreadCoAPServer.begin();
OThreadCoAPServer.joinMulticastGroup(LAMP_GROUP);   // ff03::abcd

// 3) onLamp(): multicast PUT -> set lampTarget, no reply (RFC 7252 §8.2)
//    loop(): animateLamp(lampTarget) when target changes
```

## CoAP resource

| Method | Path | Payload | Behavior |
| --- | --- | --- | --- |
| GET | `Lamp` | — | Unicast only: returns `"0"` or `"1"` |
| PUT | `Lamp` | `"0"` or `"1"` | Sets lamp off/on; multicast PUT gets no reply |

Multicast PUT from switches is **fire-and-forget**. Unicast PUT/GET still receive
normal CoAP responses.

## Expected serial output

```text
=== CoAP Light (server) ===
Forming Thread network...
Waiting for attach..
Attached as Leader.
Starting Commissioner...
Commissioner ready (PSKd "J01NME")
Starting CoAP server...
Ready. CoAP resource 'Lamp' (multicast group ff03::abcd)
CoAP PUT from fdde:ad00:beef:0:.... -> ON (multicast, no reply)
```

If network setup fails temporarily:

```text
=== CoAP Light (server) ===
Forming Thread network...
Waiting for attach.........
Attach timeout.
Network setup failed, retrying in 2s...
```

## RGB LED legend

| Color / state | Meaning |
| --- | --- |
| Red at boot | Network not ready yet. |
| Green after attach | Thread attached and Commissioner started. |
| White fade | Lamp ON (CoAP PUT `"1"`). |
| Off | Lamp OFF (CoAP PUT `"0"`). |

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Joiner secret accepted by the Commissioner.                  |
| `JOINER_WINDOW_SEC`| How long `addJoiner()` stays valid (default 600 s).          |
| `CHANNEL`          | 802.15.4 channel (default 15).                               |
| `PAN_ID`           | 16-bit PAN ID (default `0xABCD`).                            |
| `EXTPANID`         | Extended PAN ID bytes.                                       |
| `NETKEY`           | 128-bit network key.                                         |
| `NETWORK_NAME`     | Thread network name string.                                  |
| `LAMP_GROUP_BYTES` | Realm-local multicast group (default `ff03::abcd`).          |

CoAP listens on port **5683**. Multicast subscription via
`joinMulticastGroup()` is required to receive group traffic.

## Troubleshooting

**Startup order:** Flash this **light (Leader + Commissioner)** sketch first.
Wait for `Commissioner ready`, then flash [CoAP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch). **Reset switches**
after any server reset.

| Symptom | Likely cause |
| --- | --- |
| `CoAP server failed` | OpenThread not attached. |
| `Failed to join multicast group` | Invalid group address or interface not up. |
| Switch join fails | Commissioner window closed — reset light or extend `JOINER_WINDOW_SEC`. |
| Switch attached but lamp unchanged | Light not subscribed to `LAMP_GROUP` or wrong group address on switch. |
| Client worked, then stops after server reset | Server `initNew()` each boot — **reset switch** to re-join. |

## See also

* [CoAP Light Switch — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/README.md) — architecture and CLI comparison.
* [CoAP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch) — Joiner + multicast CoAP client.
* [UDP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) — same lamp pattern over UDP.

## License

Apache License 2.0.
