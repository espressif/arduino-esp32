# JoinerNode - Thread Joiner (Native API)

Client side of the [Thread Commissioning demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadCommissioning/README.md). This sketch:

* starts the OpenThread stack **without** a local operational dataset,
* optionally hints the radio channel with `THREAD_CHANNEL` (default 15) to skip
  a full channel scan,
* runs the synchronous **Joiner** state machine with PSKd `J01NME` while Thread
  is **not** yet enabled,
* calls `start()` after successful commissioning to enable Thread with the
  dataset provisioned by the Commissioner,
* prints the received dataset, role, RLOC, and addresses every 5 seconds.

It is the counterpart of [Thread Commissioning — CommissionerNode (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode).

> **Important:** the Commissioner must have its joiner window **open** before this
> sketch can join. On the Commissioner Serial log, wait for
> `Press the button on GPIO ... to open the joiner window`, press the button,
> and confirm `Joiner window OPEN...` before starting or retrying this Joiner.

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

## Prerequisites

[Thread Commissioning — CommissionerNode (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) must already be running as Leader
with Commissioner `ACTIVE` on the same RF range.

The Commissioner operator must **press the BOOT button** to open the joiner
window (`Joiner window OPEN...`, valid for 120 s) before this Joiner runs
`startJoiner()`.

Set `THREAD_CHANNEL` to the same value as the Commissioner (default 15). Set
to `0` to scan every supported channel instead (slower first join).

## What the sketch does

```cpp
// 1) Stack up with NO local dataset.
threadJoinerNode.begin(false);
#if THREAD_CHANNEL
threadJoinerNode.setChannel(THREAD_CHANNEL);   // optional channel hint
#endif
threadJoinerNode.networkInterfaceUp();

// 2) Run Joiner commissioning (Thread must NOT be enabled yet).
otError err = threadJoinerNode.startJoiner("J01NME", JOIN_TIMEOUT_MS);
if (err == OT_ERROR_NONE) {
  threadJoinerNode.start();                    // enable Thread with new dataset
}

// 3) loop(): print role, RLOC, learned network key, addresses every 5 s
```

The Joiner never sees the network key on the wire in plaintext — it is delivered
inside the authenticated MeshCoP / DTLS handshake derived from the PSKd.

## Expected serial output

```text
=== Joiner Demo - Joiner Node ===
EUI-64 (Joiner Id input): 60553caab1234567
Joining with PSKd: J01NME
Joiner: commissioning SUCCESS.
Thread enabled. Waiting to attach...
==============================================
OpenThread Network Information (post-join):
Role:           Child
RLOC16:         0xa401
Network Name:   ESP_OT_Joiner
Channel:        15
PAN ID:         0x1234
Extended PAN:   dead00beef00cafe
Network Key:    00112233445566778899aabbccddeeff
Mesh Local EID: fdde:ad00:beef:0:....
Node RLOC:      fdde:ad00:beef:0:....
Leader RLOC:    fdde:ad00:beef:0:0:ff:fe00:0
```

If commissioning fails:

```text
Joiner: PSKd mismatch. Check CommissionerNode PSKD.
```

or

```text
Joiner: no joinable network found. Is the CommissionerNode running?
```

## Customization

Build-time macros at the top of the sketch (override with `-D` flags):

| Macro            | Default | Purpose                                                      |
| ---------------- | ------- | ------------------------------------------------------------ |
| `THREAD_CHANNEL` | `15`    | Channel hint (11..26). Must match Commissioner. Set `0` to scan all channels. |
| `PSKD`           | `J01NME`| Pre-Shared Key for Device. Must match Commissioner's `addJoiner()`. |
| `JOIN_TIMEOUT_MS`| `30000` | Max wait inside `startJoiner()` for the commissioner.        |

## Troubleshooting

**Startup order:** Start [Thread Commissioning — CommissionerNode (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) first. Wait
for Leader + Commissioner `ACTIVE`. **Press the BOOT button** on the Commissioner
to open the joiner window. Then flash this Joiner sketch. Reset this board if it
booted before the window was open.

| Symptom | Likely cause |
| --- | --- |
| `OT_ERROR_SECURITY` | PSKd does not match the Commissioner's `addJoiner()` call. |
| `OT_ERROR_NOT_FOUND` | No joinable network — Commissioner not running, window not open, or window expired. |
| `OT_ERROR_RESPONSE_TIMEOUT` | Commissioner started the handshake but did not finish in time — increase `JOIN_TIMEOUT_MS` or retry. |
| Joiner started before Commissioner ready | Reset this board after Commissioner opens the join window. |
| Slow first join | `THREAD_CHANNEL=0` scans every channel — set the same channel as Commissioner (default 15). |

## See also

* [Thread Commissioning — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadCommissioning/README.md) — how to run both boards and open the joiner window.
* [Thread Commissioning — CommissionerNode (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) — companion Leader + Commissioner sketch.
* [UDP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch) — Joiner with NVS resume and UDP lamp client.

## License

Apache License 2.0.
