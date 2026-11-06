# light - Thread Leader + Commissioner + UDP Server

Server side of the [UDP Light + Switch demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/README.md). This sketch turns the
board into:

* the **Leader** of a fresh Thread network whose operational dataset is
  hard-coded in the sketch,
* the **Commissioner** of that network, accepting any joiner that
  presents the configured PSKd,
* a **UDP server** on realm-local multicast `ff03::abcd` port 5051 that
  controls an on-board RGB "lamp" and acknowledges every command back to
  the sender.

It is the counterpart of [UDP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch).
One light can serve many switches at the same time.

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
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable `otCommissioner*` APIs used by the sketch.|

## What the sketch does

```cpp
// 1) Bring the Thread stack up. Resume the NVS dataset when present,
//    otherwise provision a complete hard-coded dataset.
OThread.begin(false);
if (!OThread.hasActiveDataset()) {
  DataSet ds; ds.initNew();
  ds.setNetworkName("ESP_OT_UDP_IoT");
  ds.setChannel(15);
  ds.setPanId(0xABCD);
  ds.setExtendedPanId(...);
  ds.setNetworkKey(...);
  OThread.commitDataSet(ds);
}
OThread.networkInterfaceUp();
OThread.start();

// 2) Wait until we are Leader / Router / Child.
// 3) Petition Commissioner and add a wildcard joiner.
OThread.startCommissioner();
OThread.addJoiner("J01NME", 600 /* seconds */);

// 4) Bind UDP server on ff03::abcd:5051.
const uint8_t groupBytes[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
OtUdp.beginMulticast(IPAddress(IPv6, groupBytes), 5051);

// 5) loop(): parsePacket() -> apply -> beginPacket(src,port) ACK -> endPacket().
```

## Wire protocol it accepts

| Incoming payload | Action                              | Reply       |
| ---------------- | ----------------------------------- | ----------- |
| `ON`             | `applyLamp(true)`                   | `ACK ON`    |
| `OFF`            | `applyLamp(false)`                  | `ACK OFF`   |
| `TOGGLE`         | `applyLamp(!lampOn)`                | `ACK ON/OFF`|
| `STATUS`         | (no change, query only)             | `ACK ON/OFF`|
| anything else    | logged and dropped                  | (no ACK)    |

ACKs are sent **unicast** back to `OtUdp.remoteIP() : OtUdp.remotePort()` so
that only the switch that issued the command sees the response.

## Expected serial output

```text
=== light: Bringing up Thread network ===
Thread started, waiting for attached role...
.....
Attached as Leader.
Petitioning Commissioner...
Commissioner ACTIVE - PSKd "J01NME" accepted for 600 s.
Flash switch sketches now.
Listening on [ff03:0:0:0:0:0:0:abcd]:5051 (and unicast)
Mesh-Local EID: fdde:ad00:beef:0:....

RX [fdde:ad00:beef:0:abcd:...]:5051 -> 'TOGGLE'
RX [fdde:ad00:beef:0:abcd:...]:5051 -> 'TOGGLE'
Role=Leader lamp=ON
```

## RGB LED legend

Before the first valid switch packet, the light LED shows connection status.
After a switch sends `ON`, `OFF`, `TOGGLE`, or `STATUS`, the LED represents only
the lamp output.

| Color            | Meaning                                                     |
| ---------------- | ----------------------------------------------------------- |
| Solid red        | Not connected, reconnecting, or setup failed. Check Serial. |
| Solid green      | Thread is connected and UDP is ready, waiting for the first switch packet. |
| Smooth fade up   | Lamp is turning ON after a switch command.                  |
| Bright white     | Lamp ON.                                                    |
| Smooth fade down | Lamp is turning OFF after a switch command.                 |
| Off              | Lamp OFF after a switch command.                            |

## Customization

All tunables are at the top of the `.ino` file:

| Constant            | Purpose                                                    |
| ------------------- | ---------------------------------------------------------- |
| `PSKD`              | Pre-Shared Key for Device given out via `addJoiner`.       |
| `JOINER_WINDOW_SEC` | How long, after every boot, new joiners are accepted.      |
| `OT_CHANNEL`        | 802.15.4 channel of the network (11..26).                  |
| `OT_PAN_ID`         | 16-bit PAN ID.                                             |
| `OT_EXTPANID`       | 64-bit extended PAN ID.                                    |
| `OT_NETKEY`         | 128-bit network key (delivered to joiners over the air).   |
| `OT_NETWORK_NAME`   | Friendly network name.                                     |
| `LIGHT_GROUP`       | Multicast group the light subscribes to (default `ff03::abcd`). |
| `LIGHT_PORT`        | UDP port the light listens on (default 5051).              |

If you change `LIGHT_GROUP`, `LIGHT_PORT`, `OT_CHANNEL` or `PSKD` make
sure to mirror the change in [UDP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch).

`LIGHT_PORT` intentionally avoids OpenThread-reserved ports: 5683/5684 are
CoAP/CoAPs and 61631 is Thread TMF CoAP.

## Why `DataSet` and not live setters for the Leader?

The `OpenThread` class exposes two ways of pushing radio / network
parameters into the stack:

1. **`DataSet`** - `ds.initNew()` + `setNetworkName/...` +
   `OThread.commitDataSet(ds)`. This is what `networkSetup()` in
   `light.ino` uses only when no active dataset is already stored in NVS.
2. **Live setters** - `OThread.setChannel/setPanId/setExtendedPanId/`
   `setNetworkKey/setNetworkName(...)`. This is what `switch.ino`
   uses for the channel hint, and what the OpenThread CLI's
   `dataset channel` / `dataset networkkey` style commands map to.

They are NOT interchangeable for a node that has to **form a new Thread
network** (i.e. become Leader). The reason is that a Thread node will
only accept `otThreadSetEnabled(true)` if a **complete** Active
Operational Dataset is committed. A complete dataset contains many more
fields than the five "interesting" ones a user typically wants to set:

| Field                 | Set by `DataSet::initNew()` | Set by live setters? |
| --------------------- | --------------------------- | -------------------- |
| Network Key           | random                      | yes (`setNetworkKey`)|
| Extended PAN ID       | random                      | yes                  |
| PAN ID                | random                      | yes                  |
| Channel               | platform default            | yes                  |
| Network Name          | `"OpenThread-XXXX"` random  | yes                  |
| **Active Timestamp**  | monotonic counter           | **no**               |
| **PSKc**              | random commissioner cred.   | **no**               |
| **Mesh-Local Prefix** | random ULA `fd...::/64`     | **no**               |
| **Channel Mask**      | platform default            | **no**               |
| **Security Policy**   | hard-coded defaults         | **no**               |

The "no" rows are mandatory for a Thread node to become Leader, but the
live setters do not synthesize them. On a fresh node with empty NVS, the
`OThread.start()` call would either refuse to start or fall back to
default-random values that change at every boot.

That is why this sketch goes through `DataSet`:

```cpp
DataSet ds;
ds.initNew();                       // generates ALL mandatory fields
ds.setNetworkName(OT_NETWORK_NAME); // override the human-facing ones
ds.setChannel(OT_CHANNEL);
ds.setPanId(OT_PAN_ID);
ds.setExtendedPanId(OT_EXTPANID);
ds.setNetworkKey(OT_NETKEY);
OThread.commitDataSet(ds);          // atomic otDatasetSetActive()
```

After the first successful boot, the dataset is persisted in NVS
(including the auto-generated PSKc and Mesh-Local Prefix), so the same
node always comes back on the same network. To apply changed dataset
constants, erase the board's NVS or factory-reset the OpenThread dataset first.

### When are the live setters appropriate?

Two cases:

* **On the Joiner side** (`switch.ino`): there is no dataset to
  build locally - the Commissioner sends it over the air. The Joiner
  only needs a **channel hint** via `OThread.setChannel(CHANNEL)` to
  skip the full-band scan, which is exactly what `switch.ino`
  does.
* **Updating an already-attached node**: if the dataset already exists
  in NVS (e.g. set previously by `commitDataSet`, by a Joiner, or by
  the CLI), you can stop Thread and adjust an individual field with the
  live setter, then re-`start()` it.

### Live-setter-only alternative for the Leader (works but with caveats)

If you really want `networkSetup()` to look "flat" (no `DataSet`
variable), seed a complete dataset once and overwrite the fields with
live setters afterwards:

```cpp
static void networkSetup() {
  OThread.begin(false);

  DataSet seed;            // throwaway: only used to seed the mandatory
  seed.initNew();          // PSKc / mesh-local prefix / etc.
  OThread.commitDataSet(seed);

  OThread.setNetworkName(OT_NETWORK_NAME);
  OThread.setChannel(OT_CHANNEL);
  OThread.setPanId(OT_PAN_ID);
  OThread.setExtendedPanId(OT_EXTPANID);
  OThread.setNetworkKey(OT_NETKEY);

  OThread.networkInterfaceUp();
  OThread.start();
}
```

Trade-off: every cold boot generates a **fresh random PSKc and
Mesh-Local Prefix** because `initNew()` re-randomises them. The mesh
IPv6 addresses of the Leader change across resets. The light + switch
demo still works because the wire protocol uses the realm-local
multicast group `ff03::abcd` (which does not depend on the mesh-local
prefix), but tools that pinned an address (e.g. an mDNS-discovered
Border Router service) would have to re-discover the Leader after every
reset.

The current sketch keeps the `DataSet` form because it gives stable,
reproducible identities across reboots.

## Troubleshooting

**Startup order:** Flash this **light (Leader + Commissioner)** sketch first and wait for `Commissioner ACTIVE` and `Flash switch sketches now.` Then flash [UDP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch). The joiner window is **600 s** (`JOINER_WINDOW_SEC`); if it expires, reset the light or extend the window. Reset any switch that booted before Commissioner was active.

| Symptom                                                   | Likely cause                                                                          |
| --------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Switch cannot join on first boot                          | Switch started before light Commissioner was ready — start light first, then reset switch. |
| LED stays red after "Bringing up Thread network"          | Could not attach within 60 s. Verify the build has `CONFIG_OPENTHREAD_ENABLED=y`.     |
| `startCommissioner failed: 7`                             | Already a commissioner on the network. Use a single light or stop the other one.     |
| `addJoiner failed: 13`                                    | `OT_ERROR_INVALID_ARGS`: PSKd does not satisfy 6..32 chars / base32-thread alphabet.  |
| Switch joiner shows `OT_ERROR_NOT_FOUND`                  | The 600 s joining window expired. Reset the light or extend `JOINER_WINDOW_SEC`.      |
| `RX` lines print unknown commands                         | The switch is sending something other than `ON`/`OFF`/`TOGGLE`/`STATUS`.              |
| Light receives RX but switch shows "No ACK"               | Switch UDP socket may be bound on a different port. Check `LIGHT_PORT` on both sides. |
| Light detaches after a reset or RF interruption            | The sketch restarts Thread and reopens UDP on the persisted dataset.                  |

## See also

* [UDP Light Switch — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/README.md) — high-level architecture for the
  light + switch pair.
* [UDP Light Switch — switch (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch) — companion Joiner and multicast command client sketch.
* [Thread Commissioning — CommissionerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) —
  same Leader + Commissioner without UDP traffic.

## License

Apache License 2.0.
