# switch - Thread Joiner + UDP Client

Client side of the [UDP Light + Switch demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/README.md). This sketch:

* resumes a stored Operational DataSet from NVS after normal resets,
* runs the **Thread Joiner** state machine only when no stored dataset exists
  or the stored dataset cannot attach, learning the network parameters over the
  air from a `light` Commissioner using the PSKd `J01NME`,
* once joined, opens a `OThreadUDP` socket on port 5051,
* on every **BOOT-button press**, sends `"TOGGLE"` to the realm-local
  multicast group `ff03::abcd` and waits up to 1 second for the light's
  `"ACK ON"` or `"ACK OFF"` confirmation. Command and ACK details are printed
  to Serial Monitor.

It is the counterpart of [UDP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light).
You can flash this sketch on as many additional H2 / C6 / C5 boards as
you want - they will all join the same light's network and all of them
can drive the same lamp.

## Supported Targets

| SoC      | Thread | RGB LED  | BOOT Button | Status    |
| -------- | ------ | -------- | ----------- | --------- |
| ESP32-H2 | yes    | Required | `BOOT_PIN`  | Supported |
| ESP32-C6 | yes    | Required | `BOOT_PIN`  | Supported |
| ESP32-C5 | yes    | Required | `BOOT_PIN`  | Supported |

The sketch defaults `USER_BUTTON` to the board's `BOOT_PIN`. If your board's
button is on a different GPIO, override `USER_BUTTON` at the top of the sketch.

## Required IDF features (sdkconfig)

| Feature                              | Why                                              |
| ------------------------------------ | ------------------------------------------------ |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build the OpenThread stack.                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure the SoC has the 802.15.4 radio.           |
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable `otJoiner*` APIs used by the sketch.      |

## Prerequisites

A `light` instance must already be running on the same RF range.

For the switch's **first commissioning** (or after erasing NVS / factory reset),
the light must have called `addJoiner("J01NME", ...)` recently (i.e. within the
`JOINER_WINDOW_SEC` window - default 600 s).

After the switch has joined once, the dataset is stored in NVS. A later switch
reset or power loss can resume that stored dataset and reattach without the
light reopening its joiner window.

## What the sketch does

```cpp
// 1) Bring OT stack up. If a dataset is already stored in NVS, resume it.
OThread.begin(false);
if (OThread.hasActiveDataset()) {
  OThread.networkInterfaceUp();
  OThread.start();
  // wait for attach...
}

// 2) If no stored dataset exists or attach times out, run the Joiner.
OThread.setChannel(15);
OThread.networkInterfaceUp();
OThread.startJoiner("J01NME", 60000);
OThread.start();                          // enable Thread with the new dataset

// 3) Bind UDP on the same port the light replies to (5051).
OtUdp.begin(5051);

// 4) On BOOT press: send "TOGGLE" to ff03::abcd:5051 and wait for the ACK.
OtUdp.beginPacket(LIGHT_GROUP, 5051);
OtUdp.write("TOGGLE", 6);
OtUdp.endPacket();
// then parsePacket() for up to ACK_TIMEOUT_MS waiting for "ACK ON"/"ACK OFF"
```

## Expected serial output

```text
=== switch: Thread Joiner + UDP client ===
Stored Thread dataset found; resuming network without commissioning.
.....
Attached as Child.
UDP bound on port 5051
Mesh-Local EID: fdde:ad00:beef:0:....
Press BOOT to toggle the light.
```

First-time commissioning output looks similar to:

```text
=== switch: Thread Joiner + UDP client ===
No stored Thread dataset; commissioning is required.
EUI-64 = 60553caab1234567
Joining with PSKd "J01NME" (timeout 60000 ms)...
Joiner: SUCCESS
.....
Attached as Child.
UDP bound on port 5051
Mesh-Local EID: fdde:ad00:beef:0:....
Press BOOT to toggle the light.

TX [ff03:0:0:0:0:0:0:abcd]:5051 -> 'TOGGLE'
ACK from [fdde:ad00:beef:0:....]:5051 <- 'ACK ON'
TX [ff03:0:0:0:0:0:0:abcd]:5051 -> 'TOGGLE'
ACK from [fdde:ad00:beef:0:....]:5051 <- 'ACK OFF'
```

## RGB LED legend

The switch LED shows only the switch node's Thread connection state. It does
not mirror the remote lamp state and does not flash for individual commands.
Use Serial Monitor to see `TOGGLE`, ACK, and timeout details.

| Color     | Meaning                                      |
| --------- | -------------------------------------------- |
| Solid red | Not connected yet, reattaching, or setup failed. |
| Solid green | Attached to Thread and UDP socket is ready. |

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Pre-Shared Key for Device. Must match the commissioner.      |
| `CHANNEL`          | 802.15.4 channel hint (skip scan). Must match the network.   |
| `LIGHT_GROUP`      | Multicast group commands are sent to.                        |
| `LIGHT_PORT`       | UDP port the light listens on (default 5051).                |
| `ACK_TIMEOUT_MS`   | Max wait for the light's ACK packet.                         |
| `JOIN_TIMEOUT_MS`  | Max wait inside `startJoiner()` for the commissioner.        |
| `RESUME_ATTACH_TIMEOUT_MS` | Max wait for reattach using a stored NVS dataset before retrying commissioning. |
| `REATTACH_AFTER_MISSED` | Consecutive missing ACKs before forcing Thread re-attach. |
| `USER_BUTTON`      | GPIO of the BOOT / user button.                              |

`LIGHT_PORT` intentionally avoids OpenThread-reserved ports: 5683/5684 are
CoAP/CoAPs and 61631 is Thread TMF CoAP.

The switch does **not** automatically retransmit `TOGGLE` after a missing ACK.
`TOGGLE` is not idempotent: if the light applied the command but the ACK was
lost, retransmitting the same command would toggle the lamp back again.

## Troubleshooting

**Startup order:** Start [UDP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) first and wait for `Commissioner ACTIVE`. Then flash this switch sketch. The switch joins **once in `setup()`** (NVS resume or Joiner commissioning); if the light was not ready, press **reset** after Commissioner is active.

| Symptom                                                                 | Likely cause                                                                                       |
| ----------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| Switch booted before light Commissioner ready                           | Joiner window closed or no commissioner — start light first, then reset this board.                |
| `Joiner: PSKd mismatch.` (`OT_ERROR_SECURITY`)                          | `PSKD` differs from the one used in `light::addJoiner`.                                        |
| `Joiner: no joinable network found.`                                    | No commissioner. Power-cycle `light` or re-run `addJoiner`.                                    |
| `Joiner: commissioner did not respond in time.` (`OT_ERROR_RESPONSE_TIMEOUT`) | Mesh congestion or distance. Increase `JOIN_TIMEOUT_MS` and try again.                       |
| Reset switch cannot reattach long after commissioning                   | It should resume the NVS dataset first. If NVS was erased or the dataset changed, reopen the light's joiner window. |
| Joins successfully but BOOT press shows "No ACK"                        | Network attached but no light listening on `LIGHT_GROUP:LIGHT_PORT`. Check the light's serial log. |
| Multiple switches: only one gets the ACK                                | Expected: the light unicasts the ACK back only to the sender's `remoteIP/remotePort`.              |
| Repeated "No ACK" messages after a light reset                          | The switch forces a Thread re-attach after `REATTACH_AFTER_MISSED` missing ACKs.                    |

## See also

* [UDP Light Switch — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/README.md) — high-level architecture for the
  light + switch pair.
* [UDP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light) — companion Leader, Commissioner, and lamp server sketch.
* [Thread Commissioning — JoinerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) —
  same Joiner flow without UDP traffic.

## License

Apache License 2.0.
