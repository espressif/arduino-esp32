# Joiner Demo - Commissioner Node (Native API)

This example is part of the `ThreadCommissioning` pair. It plays the
role of:

- **Thread network former**: provides the operational dataset (network key,
  channel, PAN ID, ...) of a brand-new Thread network and normally becomes
  Leader.
- **Commissioner**: accepts a remote Joiner that knows only the PSKd
  (Pre-Shared Key for Device) and securely hands the dataset over to it
  during commissioning.

Run this sketch on one ESP32-H2 / ESP32-C6 / ESP32-C5 and the companion
[`../JoinerNode/JoinerNode.ino`](../JoinerNode/JoinerNode.ino) on a second
board.

> **Important:** the Commissioner role starts automatically after this node
> attaches, but it does **not** accept Joiners until you press the button.
> Wait for `Press the button on GPIO ... to open the joiner window`, then press
> `JOIN_BUTTON_PIN` (the on-board BOOT button by default). This starts the
> commissioning/joiner listening window for `JOINER_WINDOW_SEC` seconds.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | yes | Supported |
| ESP32-C6 | yes | Supported |
| ESP32-C5 | yes | Supported |

## Required IDF / sdkconfig

- `CONFIG_OPENTHREAD_ENABLED=y`
- `CONFIG_OPENTHREAD_COMMISSIONER=y`
- `CONFIG_SOC_IEEE802154_SUPPORTED=y`

## What it does

1. `OpenThread.begin(false)` - start the stack without auto-loading any NVS
   dataset.
2. Build a fresh `DataSet` (network name, channel `THREAD_CHANNEL` (default
   15), PAN 0x1234, extended PAN ID, network key) and `commitDataSet()` it.
   Pinning the channel is optional - `initNew()` already picks a valid
   (random) one - but it lets the Joiner skip the channel scan (see below).
3. `networkInterfaceUp()` + `start()` - bring up Thread; this node becomes
   the Leader of a brand-new partition.
4. Once attached, call `startCommissioner()` which petitions the role and
   blocks until `OT_COMMISSIONER_STATE_ACTIVE`.
5. **Press the button** (`JOIN_BUTTON_PIN`, defaults to the on-board BOOT
   button) to call `addJoiner(PSKD, nullptr, 120)`. This opens the joiner
   window, accepting **any** joiner that presents the PSKd `"J01NME"` for the
   next 120 seconds. Press again any time to re-open the window after it
   expires.
6. The `loop()` prints role / RLOC / addresses and the Commissioner state
   every 5 seconds.

## Configuration

These build-time macros can be overridden from the top of the sketch or with
`-D` compiler flags (e.g. in `boards.local.txt` / PlatformIO `build_flags`):

| Macro | Default | Purpose |
| ----- | ------- | ------- |
| `THREAD_CHANNEL` | `15` | IEEE 802.15.4 channel (11..26) the network is formed on. Must match the Joiner's `THREAD_CHANNEL` so it can attach without scanning. |
| `JOIN_BUTTON_PIN` | `BOOT_PIN` | GPIO of the (active-low) button that opens the joiner window. `BOOT_PIN` comes from `Arduino.h` (GPIO9 on C6/H2, GPIO28 on C5). |

## Expected serial output

```text
=== Joiner Demo - Commissioner Node ===
Thread network started, waiting to become Leader...
Attached as Leader. Petitioning Commissioner...
Commissioner ACTIVE.
Press the button on GPIO 9 to open the joiner window.
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

## Key APIs

```cpp
// Set up a network and become Leader (same as the legacy example).
threadCommissionerNode.begin(false);
dataset.initNew();
// ... configure dataset ...
threadCommissionerNode.commitDataSet(dataset);
threadCommissionerNode.networkInterfaceUp();
threadCommissionerNode.start();

// NEW: become the Commissioner so joiners can attach with a PSKd.
threadCommissionerNode.startCommissioner();    // blocks until ACTIVE

// Open the joiner window on demand, when the button is pressed.
if (joinButtonPressed()) {                     // active-low JOIN_BUTTON_PIN
  threadCommissionerNode.addJoiner("J01NME",   // PSKd
                                    nullptr,   // any joiner (wildcard)
                                    120);      // valid for 120 s
}
```

## Troubleshooting

- **`Commissioner petition failed`** - the petition is only accepted once
  the device is attached to a network. The sketch automatically waits for
  attachment before petitioning; if it still fails, increase the timeout
  (`startCommissioner(60000)`) or check that no other commissioner is
  active in the partition.
- **Joiner never appears** - first make sure you **pressed the button** to
  open the joiner window (look for `Joiner window OPEN` on the serial log). The
  window only stays open for `JOINER_WINDOW_SEC` (120 s); press the button
  again to re-open it. Then confirm both boards run on the same channel
  (`THREAD_CHANNEL`; radio is restricted to 15.4 channels 11..26) and that the
  PSKd matches exactly. The PSKd must be ASCII, 6 to 32 characters,
  base32-thread-friendly (no `0`, `I`, `O`, `Q`).

## License

Apache License 2.0.
