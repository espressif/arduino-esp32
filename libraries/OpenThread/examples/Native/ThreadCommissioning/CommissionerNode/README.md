# CommissionerNode - Thread Leader + Commissioner (Native API)

Server side of the [Thread Commissioning demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadCommissioning/README.md). This sketch:

* forms a brand-new Thread network with a hard-coded `DataSet` (network name,
  channel `THREAD_CHANNEL`, PAN ID, extended PAN ID, network key),
* becomes the **Leader** of a new partition via `networkInterfaceUp()` +
  `start()`,
* petitions the **Commissioner** role automatically once attached,
* opens the **joiner window** only when you **press the button** (`JOIN_BUTTON_PIN`,
  BOOT by default), calling `addJoiner(PSKD, 120)` to accept any Joiner that
  presents PSKd `J01NME`,
* prints role, RLOC, addresses, and Commissioner state every 5 seconds in
  `loop()`.

It is the counterpart of [Thread Commissioning — JoinerNode (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode).

> **Important:** the Commissioner role starts automatically after attach, but it
> does **not** accept Joiners until you press the button. Wait for
> `Press the button on GPIO ... to open the joiner window`, then press
> `JOIN_BUTTON_PIN`.

## Supported Targets

| SoC      | Thread | BOOT Button | Status    |
| -------- | ------ | ----------- | --------- |
| ESP32-H2 | yes    | `BOOT_PIN`  | Supported |
| ESP32-C6 | yes    | `BOOT_PIN`  | Supported |
| ESP32-C5 | yes    | `BOOT_PIN`  | Supported |

Override `JOIN_BUTTON_PIN` at the top of the sketch if your button GPIO differs.

## Required IDF features (sdkconfig)

| Feature                              | Why                                              |
| ------------------------------------ | ------------------------------------------------ |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build the OpenThread stack.                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure the SoC has the 802.15.4 radio.           |
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable Commissioner APIs.                        |

## Prerequisites

Flash this sketch **before** [Thread Commissioning — JoinerNode (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode).

After attach and `Commissioner ACTIVE`, **press the BOOT button** to open the
joiner window before starting or resetting the Joiner. The window stays open for
120 s (`JOINER_WINDOW_SEC`); press the button again to re-open it after expiry.

Pin `THREAD_CHANNEL` (default 15) so it matches the Joiner's channel hint and
avoids a full channel scan on the Joiner side.

## What the sketch does

```cpp
// 1) Form the Thread network and become Leader.
threadCommissionerNode.begin(false);
dataset.initNew();
dataset.setNetworkName("ESP_OT_Joiner");
dataset.setChannel(THREAD_CHANNEL);
dataset.setPanId(0x1234);
// ... ext PAN ID, network key ...
threadCommissionerNode.commitDataSet(dataset);
threadCommissionerNode.networkInterfaceUp();
threadCommissionerNode.start();

// 2) Petition Commissioner once attached (in loop()).
threadCommissionerNode.startCommissioner();

// 3) On button press: open joiner window.
if (joinButtonPressed()) {
  threadCommissionerNode.addJoiner("J01NME", 120);
}
```

## Expected serial output

```text
=== Joiner Demo - Commissioner Node ===
Thread network started, waiting to become Leader...
Attached as Leader. Petitioning Commissioner...
Commissioner ACTIVE.
===>>>Press the button on GPIO 9 to open the joiner window.
==============================================
Role:           Leader
RLOC16:         0x0000
Network Name:   ESP_OT_Joiner
Channel:        15
PAN ID:         0x1234
Extended PAN:   dead00beef00cafe
Mesh Local EID: fdde:ad00:beef:0:....
Leader RLOC:    fdde:ad00:beef:0:....
Node RLOC:      fdde:ad00:beef:0:....
Commissioner:   ACTIVE
   <-- press the BOOT button here -->
Joiner window OPEN: PSKd "J01NME" accepted for 120 s.
Bring up the JoinerNode sketch now.
```

## Customization

Build-time macros at the top of the sketch (override with `-D` flags):

| Macro             | Default    | Purpose                                                      |
| ----------------- | ---------- | ------------------------------------------------------------ |
| `THREAD_CHANNEL`  | `15`       | 802.15.4 channel (11..26). Must match Joiner's `THREAD_CHANNEL`. |
| `JOIN_BUTTON_PIN` | `BOOT_PIN` | GPIO of active-low button that opens the joiner window.      |
| `PSKD`            | `J01NME`   | Pre-Shared Key for Device accepted by `addJoiner()`.          |
| `JOINER_WINDOW_SEC` | `120`    | How long each `addJoiner()` entry stays valid.               |

Pinning the channel is optional — `initNew()` already picks a valid channel —
but matching the Joiner hint avoids a channel scan.

## Troubleshooting

**Startup order:** Flash this Commissioner sketch first. Wait for Leader +
Commissioner `ACTIVE`. **Press the BOOT button** before flashing
[Thread Commissioning — JoinerNode (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode). Press again to re-open the window after 120 s.

| Symptom | Likely cause |
| --- | --- |
| `Commissioner petition failed` | Device not attached yet, or another Commissioner is active — retry or stop the other Commissioner. |
| Joiner never appears | Joiner window not open — press the button and look for `Joiner window OPEN` before starting JoinerNode. |
| Join window expired | Window lasts 120 s — press the button again, then reset the Joiner. |
| `addJoiner failed` | Invalid PSKd — ASCII 6–32 chars, base32-thread alphabet (no `0`, `I`, `O`, `Q`). |
| Joiner on wrong channel | Both sketches must use the same `THREAD_CHANNEL`. |

## See also

* [Thread Commissioning — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadCommissioning/README.md) — how to run both boards and open the joiner window.
* [Thread Commissioning — JoinerNode (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) — companion Joiner sketch.
* [UDP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) — Commissioner with UDP lamp server.

## License

Apache License 2.0.
