# switch - Thread Joiner + CoAP Lamp Client

Client side of the [CoAP Light Switch demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/README.md). This sketch:

* runs the **Thread Joiner** state machine in `setup()` using PSKd `J01NME`
  and a channel hint, then calls `start()` after successful commissioning,
* on every **BOOT-button press**, toggles local lamp state and sends a
  **non-confirmable** CoAP PUT `"0"` or `"1"` to realm-local multicast
  `ff03::abcd` on path `Lamp`,
* uses `sendNonBlocking()` — fire-and-forget with no response (RFC 7252 §8.2),
* shows connection status on the RGB LED (red while joining/detached, dim blue
  when attached).

It is the counterpart of [CoAP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light). Flash on as many
boards as needed — all can control the same lamp.

## Supported Targets

| SoC      | Thread | RGB LED  | BOOT Button | Status    |
| -------- | ------ | -------- | ----------- | --------- |
| ESP32-H2 | yes    | Optional | `BOOT_PIN`  | Supported |
| ESP32-C6 | yes    | Optional | `BOOT_PIN`  | Supported |
| ESP32-C5 | yes    | Optional | `BOOT_PIN`  | Supported |

The sketch defaults `USER_BUTTON` to the board's `BOOT_PIN`. Override
`USER_BUTTON` at the top of the sketch if your button GPIO differs.

## Required IDF features (sdkconfig)

| Feature                              | Why                                              |
| ------------------------------------ | ------------------------------------------------ |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build the OpenThread stack.                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure the SoC has the 802.15.4 radio.           |
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable `otJoiner*` APIs used by the sketch.      |

## Prerequisites

A [CoAP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) instance must already be running on the same RF range with
`Commissioner ready (PSKd "J01NME")` and
`Ready. CoAP resource 'Lamp' (multicast group ff03::abcd)`.

The switch joins **only in `setup()`** (retries with `stop()` between attempts).
If the light was not ready when the switch booted, press **reset** after the
light Commissioner is active.

After a **light reset**, the light forms a new network (`initNew()`) — reset
the switch to re-join.

## What the sketch does

```cpp
// 1) Join the light's network via Commissioner.
OThread.setChannel(CHANNEL_HINT);
OThread.networkInterfaceUp();
OThread.startJoiner("J01NME", JOIN_TIMEOUT_MS);
OThread.start();

// 2) On BOOT press: toggle and send NON multicast PUT.
lampOn = !lampOn;
CoapClient.sendNonBlocking(LAMP_GROUP, OT_COAP_REQ_PUT, "Lamp",
                           lampOn ? "1" : "0");
// No response expected — fire-and-forget (RFC 7252 §8.2)
```

For a **confirmed** result to one lamp, use unicast instead:
`CoapClient.PUT(lampUnicastAddr, "Lamp", payload)`.

## Expected serial output

```text
=== CoAP Switch (client) ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Waiting for attach..
Attached as Child.
Ready. Press the button to toggle the lamp.
PUT Lamp=1 -> sent (multicast NON, fire-and-forget)
PUT Lamp=0 -> sent (multicast NON, fire-and-forget)
```

If join fails:

```text
=== CoAP Switch (client) ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
startJoiner failed: 7
Join failed, retrying in 3s...
```

## RGB LED legend

| Color | Meaning |
| --- | --- |
| Red | Joining or not attached. |
| Dim blue | Attached to Thread. |
| Red flash | CoAP send failed (not attached or TX queue full). |

The LED does **not** mirror the remote lamp state — check the light board or
Serial output on the light for lamp changes.

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Pre-Shared Key for Device. Must match the light Commissioner.  |
| `CHANNEL_HINT`     | 802.15.4 channel hint (default 15). Must match the network.  |
| `JOIN_TIMEOUT_MS`  | Max wait inside `startJoiner()` for the commissioner.        |
| `LAMP_GROUP_BYTES` | Multicast group for PUT commands (default `ff03::abcd`).       |
| `USER_BUTTON`      | GPIO of the BOOT / user button.                              |

CoAP uses port **5683** internally. Multicast PUT is intentionally
non-confirmable — there is no ACK to wait for.

## Troubleshooting

**Startup order:** Start [CoAP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) first and wait for
`Commissioner ready`. This switch joins **only in `setup()`** — reset this
board if it booted before the light was ready.

| Symptom | Likely cause |
| --- | --- |
| `startJoiner failed` | Light not running or Commissioner window closed — start light first, then reset switch. |
| `Join failed, retrying in 3s...` | Same as above — wait for light Commissioner, then reset. |
| `CoAP send failed — is the node attached?` | Thread detached or TX queue full. |
| Attached but lamp unchanged | Light not subscribed to `LAMP_GROUP` or light not running. |
| Worked once, fails after light reset | Light `initNew()` each boot — **reset switch** to re-join. |

## See also

* [CoAP Light Switch — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/README.md) — architecture and CLI comparison.
* [CoAP Light Switch — light (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/light) — companion Leader + Commissioner + lamp server.
* [UDP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch) — same lamp pattern over UDP with ACKs.

## License

Apache License 2.0.
