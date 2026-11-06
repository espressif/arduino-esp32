# Thread Commissioning (Joiner + Commissioner) — Native API

A two-board demo of secure Thread commissioning with the **Native API**. Instead
of sharing the network key in the sketch, a brand-new device obtains the
operational dataset over the air using only a shared secret (the **PSKd**),
delivered inside an authenticated DTLS handshake.

| Sketch | Role |
| --- | --- |
| [CommissionerNode (Leader + Commissioner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) | Forms the network, becomes Leader, petitions **Commissioner**, opens the joiner window on **button press** |
| [JoinerNode (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) | New device with **no** local dataset; runs the **Joiner** state machine to obtain the dataset via the PSKd |

The network key never travels on the wire in plaintext — it is derived from the
PSKd during the MeshCoP handshake.

## How to Run

1. Flash [CommissionerNode (Leader + Commissioner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) on one
   ESP32-H2 / ESP32-C6 / ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait until it attaches as Leader and the Commissioner becomes `ACTIVE`.
3. **Press the BOOT button** on the Commissioner to open the joiner window
   (accepts PSKd `J01NME` for 120 s).
4. Flash [JoinerNode (Joiner client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) on a second board. It runs
   `startJoiner()`, then `start()` with the provisioned dataset.

Keep both boards on the same `THREAD_CHANNEL` (default 15) so the Joiner can
attach without a full channel scan.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `CommissionerNode` |
| `CONFIG_OPENTHREAD_JOINER=y` | `JoinerNode` |

## Troubleshooting

**Startup order:** Flash [Thread Commissioning — CommissionerNode (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) first and wait
for Leader + Commissioner `ACTIVE`. **Press the BOOT button** to open the joiner
window (PSKd `J01NME`, 120 s). Then flash [Thread Commissioning — JoinerNode (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode). If the
Joiner booted before the window was open, press **reset** on the Joiner after
opening the window.

| Symptom | Likely cause |
| --- | --- |
| Joiner never attaches | Commissioner join window not open — button must be pressed before starting Joiner. |
| Joiner started before Commissioner ready | Reset Joiner after Commissioner is Leader and join window is open. |
| `Commissioner petition failed` | Device not attached yet or another Commissioner active in the partition. |
| Join works once, fails on retry | Join window expired (120 s) — press button again on Commissioner, then reset Joiner. |
| Boards on different channels | Both sketches must use the same `THREAD_CHANNEL` — check Serial for channel mismatch. |

## See Also

* [Simple Thread Network examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork) — simpler bring-up that shares the network key directly (no commissioning).
* [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) — same commissioning flow with UDP lamp control.
* [Native examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md) — all Native API example categories.
* [OpenThread library README — Joiner and Commissioner](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/README.md) — API reference and timeout details.

## License

Apache License 2.0.
