# Joiner Demo - Joiner Node (Native API)

This example is part of the `ThreadCommissioning` pair. It plays the
role of a brand-new device that has **never** been provisioned: it does not
know the network key, the PAN ID or the network name. It only knows a
shared secret (the PSKd) and uses the Thread Joiner role to obtain the
operational dataset from a Commissioner.

Run this sketch on a second ESP32-H2 / ESP32-C6 / ESP32-C5 while the
companion [`../CommissionerNode/CommissionerNode.ino`](../CommissionerNode/CommissionerNode.ino) is
already up and has reached the "Bring up the JoinerNode sketch now." line.

> **Important:** the CommissionerNode must have its joiner window open before
> this sketch can join. On the CommissionerNode serial log, wait for
> `Press the button on GPIO ... to open the joiner window`, press the configured
> button (`JOIN_BUTTON_PIN`, BOOT by default), and confirm it prints
> `Joiner window OPEN...` before starting or retrying this JoinerNode.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | yes | Supported |
| ESP32-C6 | yes | Supported |
| ESP32-C5 | yes | Supported |

## Required IDF / sdkconfig

- `CONFIG_OPENTHREAD_ENABLED=y`
- `CONFIG_OPENTHREAD_JOINER=y`
- `CONFIG_SOC_IEEE802154_SUPPORTED=y`

## What it does

1. `OpenThread.begin(false)` - start the stack without loading any dataset.
2. (Optional) `setChannel(THREAD_CHANNEL)` to pre-set the radio channel and
   skip the channel scan so the joiner attaches faster. It must match the
   CommissionerNode's `THREAD_CHANNEL`. Set `THREAD_CHANNEL` to 0 to scan every
   channel instead.
3. `networkInterfaceUp()` - required by `otJoinerStart()`.
4. `startJoiner(PSKD)` runs the synchronous Joiner state machine. The
   commissioner provisions us with the active dataset over MeshCoP.
5. `start()` enables the Thread protocol with the just-installed dataset.
6. The `loop()` prints the dataset we received and role/RLOC/IP info every
   5 seconds.

## Expected serial output

```text
=== Joiner Demo - Joiner Node ===
EUI-64 (Joiner Id input): 60553caab1234567
Joining with PSKd: J01NME
Joiner: commissioning SUCCESS.
Thread enabled. Waiting to attach...
==============================================
OpenThread Network Information (post-join):
Role:           Child       <- then Router after the upgrade jitter
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

The joiner never sees the network key on the wire. It is delivered inside
a DTLS handshake that derives its keys from the PSKd.

## Key APIs

```cpp
// New device: stack up, NO dataset committed locally.
threadJoinerNode.begin(false);
threadJoinerNode.setChannel(THREAD_CHANNEL);  // optional channel hint
threadJoinerNode.networkInterfaceUp();        // IPv6 must be up

otError err = threadJoinerNode.startJoiner("J01NME");
if (err == OT_ERROR_NONE) {
  threadJoinerNode.start();              // enable Thread protocol
}
```

## Troubleshooting

- **`OT_ERROR_SECURITY`** - the PSKd presented by this sketch does not match
  the one in the commissioner's `addJoiner()` call. Make sure both use the
  exact same string.
- **`OT_ERROR_NOT_FOUND`** - no commissioner is currently advertising a
  joinable network on any scanned channel. Confirm the CommissionerNode sketch is
  running and has printed "Joiner window OPEN...".
- **`OT_ERROR_RESPONSE_TIMEOUT`** - the commissioner started the handshake
  but did not complete it in time. Try again, or bump `JOIN_TIMEOUT_MS`.
- **Slow first join** - if you set `THREAD_CHANNEL` to 0 (or otherwise skip
  the `setChannel()` hint) the joiner scans every supported 802.15.4 channel,
  which adds a few seconds.

## Configuration

This build-time macro can be overridden from the top of the sketch or with a
`-D` compiler flag (e.g. in `boards.local.txt` / PlatformIO `build_flags`):

| Macro | Default | Purpose |
| ----- | ------- | ------- |
| `THREAD_CHANNEL` | `15` | IEEE 802.15.4 channel (11..26) hint used to attach. Must match the CommissionerNode's `THREAD_CHANNEL`. Set to `0` to scan every channel in the supported mask instead. |

## License

Apache License 2.0.
