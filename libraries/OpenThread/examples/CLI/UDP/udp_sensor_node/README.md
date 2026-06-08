# OpenThread UDP Sensor Node (CLI)

This sketch is the sensor side of the CLI-based UDP sensor demo.

It:

- configures Thread with CLI commands,
- joins/attaches as a child router-capable node (typically child),
- optionally sets Sleepy End Device behavior via CLI,
- sends periodic UDP sensor frames to the leader,
- waits for ACK and reports status to Serial.

This example uses CLI UDP commands only (no `OThreadUDP` object).
It also checks build-time sdkconfig support before applying sleepy-only setup.

## Supported Targets

| SoC | Thread | Status |
| --- | ------ | ------ |
| ESP32-H2 | ✅ | Fully supported |
| ESP32-C6 | ✅ | Fully supported |
| ESP32-C5 | ✅ | Fully supported |

## Required IDF features (sdkconfig)

| Feature | Why |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Build OpenThread stack |
| `CONFIG_OPENTHREAD_CLI=y` | Required by this sketch (`OThreadCLI`, `udp ...` commands) |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Enable IEEE 802.15.4 radio support |

Recommended for lower power behavior:

| Feature | Why |
| --- | --- |
| `CONFIG_OPENTHREAD_MTD=y` | Builds as Minimal Thread Device (end-device profile) |
| `CONFIG_IEEE802154_ENABLED=y` | 802.15.4 MAC/radio enabled in IDF |
| `CONFIG_IEEE802154_SLEEP_ENABLE=y` | Enables MAC/radio sleep support (light-sleep oriented) |
| `CONFIG_PM_ENABLE=y` | Enables ESP-IDF power management (required for 802.15.4 sleep) |

## CLI setup sequence used

Base dataset/network setup:

```text
thread stop
ifconfig down
dataset clear
dataset networkname ESP_OT_SENSORS
dataset channel 15
dataset panid 0xabcf
dataset extpanid ce010000deadc0de
dataset networkkey 102030405060708090a0b0c0d0e0f002
dataset commit active
```

Sleepy mode configuration (`USE_SLEEPY_MODE=true`):

```text
mode n
pollperiod 1000
childtimeout 300
```

Bring-up:

```text
ifconfig up
thread start
udp open
udp bind :: 5050
```

## Sleep behavior

`mode n` clears rx-on-when-idle so the node behaves as sleepy child.

- `pollperiod 1000` -> parent poll every 1 second
- `childtimeout 300` -> parent keeps child for 300 seconds without polls

OpenThread handles buffering/polling for this sleepy child model.

## Sleepy mode deep dive

This sketch uses **protocol-level sleepy behavior** (Thread SED-like behavior):

- radio stays off between polls (`rx-on-when-idle = false`),
- parent buffers indirect traffic,
- child polls parent at `pollperiod`.

Important: this sketch does **not** put the MCU into deep sleep. It keeps the
application task running and idles with `delay(...)`. You still get Thread
sleepy-child behavior, but not full chip deep-sleep current.

### Runtime sequence that makes sleepy mode work

1. Configure device mode (`mode n`) so rx-on-when-idle is disabled.
2. Configure poll interval (`pollperiod 1000`).
3. Configure parent retention window (`childtimeout 300`).
4. Start interface and Thread (`ifconfig up`, `thread start`).
5. Keep sending at an interval that gives enough time for parent polling.

The sketch applies this sequence only when build support is detected.
Otherwise it logs a warning and continues as a regular child.

### Verify sleepy state at runtime (manual checks)

After boot, from a CLI console run:

```text
mode
pollperiod
childtimeout
state
```

Expected:

- `state` is `child` (or occasionally `router`/`detached` during transitions),
- `pollperiod` matches your configured value,
- `childtimeout` matches your configured value,
- `mode` shows rx-on-when-idle is **not** enabled.

The sketch also prints:

```text
Sleepy mode build support: enabled
```

If support is missing, it prints:

```text
Sleepy mode requested but build lacks required sdkconfig flags.
Need: CONFIG_OPENTHREAD_MTD=y + CONFIG_IEEE802154_SLEEP_ENABLE=y + CONFIG_PM_ENABLE=y
Continuing as a regular child node.
Sleepy mode build support: not enabled
```

### Tuning guidance

- Lower `SED_POLL_MS` -> faster downlink response, higher power.
- Higher `SED_POLL_MS` -> lower power, slower response.
- `CHILD_TIMEOUT_S` should be several times larger than poll period to avoid
  frequent detach/reattach.
- If your ACK timeout is tight, poll period must be short enough that ACKs can
  be retrieved before `ACK_TIMEOUT_MS`.

### Recovering when the collector reboots

A Sleepy End Device only communicates through its parent. If the collector
(the leader/parent) reboots, the node can keep polling a parent that is gone
until `CHILD_TIMEOUT_S` expires, which looks like "the node never reconnects".

To recover quickly the node counts consecutive samples with no ACK and, after
`REATTACH_AFTER_MISSED` of them, forces a clean re-attach (`thread stop` /
`thread start`) and re-opens its UDP socket. It then rescans and attaches to the
rebooted collector instead of waiting out the child timeout. Lowering
`CHILD_TIMEOUT_S` also speeds up native parent-loss detection, at the cost of
the parent dropping the child sooner during normal sleep gaps.

### Build guard used by the sketch

The sketch checks this condition at compile time:

```cpp
CONFIG_OPENTHREAD_MTD && CONFIG_IEEE802154_SLEEP_ENABLE && CONFIG_PM_ENABLE
```

If this condition is false, sleepy-only commands are skipped to avoid giving a
false impression that low-power behavior is active.

## ESP-IDF folder check (what was verified)

The ESP-IDF code in this repository confirms the following:

- In `components/openthread/Kconfig`, OpenThread device type defaults to
  `CONFIG_OPENTHREAD_FTD=y` (FTD default).
- In the same Kconfig, `CONFIG_OPENTHREAD_CLI` default is enabled when
  OpenThread is enabled.
- In `examples/openthread/ot_sleepy_device/light_sleep/sdkconfig.defaults`,
  Espressif's own sleepy example enables:
  - `CONFIG_OPENTHREAD_MTD=y`
  - `CONFIG_OPENTHREAD_CLI=y`
  - `CONFIG_IEEE802154_ENABLED=y`
  - `CONFIG_IEEE802154_SLEEP_ENABLE=y`

That is consistent with this sketch's sleepy model. If your Arduino core build
is still FTD (default), this example can still run as a sleepy child via CLI
mode/poll commands; MTD is recommended for a more end-device-oriented footprint.

## Wire protocol

TX payload:

```text
id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
```

ACK expected (the value is the collector's authoritative last-stored sequence
for this node, not necessarily the one just sent):

```text
OK,<nodeId>,<collectorLastSeq>
```

## Reliable delivery (application-level ACK + collector-driven sequence)

The Thread link layer (MAC ACKs, mesh forwarding) can look healthy even when the
collector application is gone, so it is not a reliable signal that data was
*received and processed*. This sketch therefore treats the collector's
**application-level ACK as the only ground truth of delivery**, and makes the
**collector the single source of truth for the sequence number**:

- The frame for a sample uses `seq = lastConfirmedSeq + 1`.
- `sendFrameAndWaitAck()` only considers an ACK that is a real UDP-receive line,
  whose source is **not** one of this node's own addresses (so a looped-back
  frame can never be mistaken for an ACK), and whose `id` field matches ours. It
  then reads the **sequence number out of the ACK**.
- After a valid ACK the node **resyncs** its local counter to the collector's
  reported sequence: it rolls back if it had run ahead, or skips forward if the
  collector ignored a stale frame. So the node can never drift away from the
  collector's view.
- If the ACK sequence equals the frame just sent, the sample is `ACKED`. If it
  differs, the node logs a `Resync:` line and reports `RESYNC` for that sample.
- If **no** ACK arrives at all (collector off/unreachable), the sequence is
  **held** (not advanced), the same reading is retransmitted up to `TX_RETRIES`
  times, and the sample is reported `NO_ACK (sequence held)` - the count never
  moves forward without proof of delivery.

### "It still shows ACKED after I turned the collector off"

When the node prints `status=ACKED`, the collector’s ACK carried the same sequence number as the frame just sent (`OK,<id>,<collectorLastSeq>` where `collectorLastSeq == seq`). The collector can only generate such an ACK after actually receiving that datagram, so if the node keeps printing `status=ACKED` for brand-new sequence numbers, a device really is receiving and answering them — a powered-off board cannot invent new sequence numbers. Check:

- The collector is **truly unpowered**, not just reset or with its serial monitor
  closed. Over USB the board stays powered and keeps ACKing; a reset makes it
  resume the saved network from NVS and keep ACKing. Unplug it (or hold it in
  reset) to actually stop it.
- The `ACK ... <-` source address is **not** one of the node's own addresses.
  The sketch prints `My Mesh-Local EID` and `My own addresses` at startup, and
  logs `Ignoring datagram from own address ...` if it ever sees a self-sourced
  frame. If the ACK source matches one of your own addresses, you are seeing
  loopback, not a remote collector.
- No second board on the bench is running the collector sketch.

### Node reset

The node id is derived from the factory EUI-64, so it is stable across a reset,
but `s_seq` restarts at 0 (next frame `seq=1`). The collector still holds the
old (higher) sequence for that id, so it recognizes `seq=1` as a **restart**,
follows the node back to 1, logs `node <id> restarted: sequence reset to 1`, and
ACKs `1`. The node then continues normally from there. (Other backward jumps are
treated as stale and ignored, and the node is told the collector's real last seq
so it resyncs forward.)

## Expected serial output

```text
=== udp_sensor_node (CLI): Sleepy UDP sensor ===
...
Role: Child
Node ID: 3CAAB123
Collector group: ff03::abcd:5050
TX [ff03::abcd]:5050 -> 'id=3CAAB123,seq=1,temp_centi=2312,batt_mv=3820'
ACK [fd..]:5050 <- 'OK,3CAAB123,1'
sample seq=1 temp=23.12C batt=3820mV status=ACKED
```

With the collector powered off you instead see the sequence held:

```text
TX [ff03::abcd]:5050 -> 'id=3CAAB123,seq=2,temp_centi=2298,batt_mv=3810'
TX [ff03::abcd]:5050 -> 'id=3CAAB123,seq=2,temp_centi=2298,batt_mv=3810' (retransmit)
ACK timeout (no valid ACK from collector)
sample seq=2 temp=22.98C batt=3810mV status=NO_ACK (sequence held)
```

## Customization

Tune these constants in `udp_sensor_node.ino`:

- `SAMPLE_PERIOD_MS`
- `ACK_TIMEOUT_MS` (how long to wait for the collector's ACK per attempt)
- `TX_RETRIES` (immediate resends of the same sequence before reporting NO_ACK)
- `USE_SLEEPY_MODE`
- `SED_POLL_MS`
- `CHILD_TIMEOUT_S`

## License

Apache License 2.0.
