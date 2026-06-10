# OpenThread CLI UDP Sensor Fleet (CLI-only)

This document describes the **CLI/UDP** example pair in this folder and the
OpenThread CLI commands used by each sketch.

## Example goal

Demonstrate a many-to-one Thread telemetry topology where:

- one node acts as a **collector** (network leader + UDP sink),
- many nodes act as **sensors** (child/sleepy child),
- all application traffic uses **OpenThread CLI UDP commands** (`udp ...`),
- no Native UDP class/object is required.

## Sketches in this folder

| Sketch | Role |
| --- | --- |
| [`udp_sensor_collector/udp_sensor_collector.ino`](udp_sensor_collector/) | Central collector node (Leader) |
| [`udp_sensor_node/udp_sensor_node.ino`](udp_sensor_node/) | Sensor node (Child / Sleepy End Device) |

## High-level design

- **Control plane**: OpenThread CLI commands via `OThreadCLI`.
- **Data plane**: OpenThread CLI UDP commands and parsing of CLI async output.
- **Transport model**:
  - Sensor sends telemetry to realm-local multicast `ff03::abcd:5050`.
  - Collector receives payload, updates in-memory state, sends unicast ACK.

## Topology

```text
sensor node 1 ----\
sensor node 2 -----+--> udp_sensor_collector (Leader) --> Serial logs
sensor node N ----/
```

## Shared wire format

Sensor -> Collector payload:

```text
id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
```

Collector -> Sensor ACK:

```text
OK,<nodeId>,<seq>
```

## CLI commands used by collector

### Thread setup sequence

The collector only provisions a dataset on its **first** boot. On later boots it
detects the dataset OpenThread persisted in NVS with the Arduino
`OThread.hasActiveDataset()` API and simply resumes the same network, so a leader
reset restores the identical network and children re-attach to the returning
parent quickly.

First boot (no stored dataset) - note the pinned mesh-local prefix and active
timestamp, which keep the network byte-for-byte identical across re-creations:

```text
thread stop
ifconfig down
dataset clear
dataset init new
dataset networkname ESP_OT_SENSORS
dataset channel 15
dataset panid 0xabcf
dataset extpanid ce010000deadc0de
dataset networkkey 102030405060708090a0b0c0d0e0f002
dataset meshlocalprefix fdde:ad00:beef:0::
dataset activetimestamp 1
dataset commit active
ifconfig up
thread start
```

Subsequent boots (dataset already persisted):

```text
ifconfig up
thread start
```

> Because the dataset is persisted, editing the `OT_*` constants has no effect
> until you clear the stored dataset (CLI `factoryreset`, or erase flash).

### UDP setup sequence

```text
udp close
udp open
ipmaddr add ff03::abcd
udp bind :: 5050
```

### Runtime send command (ACK)

```text
udp send <sensor-ipv6> <sensor-port> OK,<nodeId>,<seq>
```

## CLI commands used by sensor

### Thread setup sequence

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

### Sleepy child configuration (when enabled in sketch)

```text
mode n
pollperiod 1000
childtimeout 300
```

### Start + UDP setup

```text
ifconfig up
thread start
udp close
udp open
udp bind :: 5050
```

### Runtime send command (telemetry)

```text
udp send ff03::abcd 5050 id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
```

## How RX is handled (CLI output parsing)

OpenThread CLI emits incoming UDP lines in this format:

```text
<len> bytes from <ipv6> <port> <payload>
```

Both sketches parse these lines from `OThreadCLI` stream:

- collector parses telemetry packets,
- sensor parses ACK packets and matches expected `OK,<nodeId>,<seq>`.

## Sleepy mode requirements (important)

The `udp_sensor_node` sketch contains optional sleepy-child logic
(`mode n` + `pollperiod` + `childtimeout`), but true low-power behavior depends
on build-time `sdkconfig` options.

To reliably use sleepy functionality, build Arduino as an **ESP-IDF component**
project and ensure these options are configured in that project's sdkconfig:

- `CONFIG_OPENTHREAD_ENABLED=y`
- `CONFIG_OPENTHREAD_CLI=y`
- `CONFIG_OPENTHREAD_MTD=y` (recommended for end-device profile)
- `CONFIG_IEEE802154_ENABLED=y`
- `CONFIG_IEEE802154_SLEEP_ENABLE=y` (802.15.4 light-sleep support)
- `CONFIG_PM_ENABLE=y` (power management required for 802.15.4 sleep)

Why this matters:

- CLI commands can still run without these settings, but they may not map to
  real low-power radio behavior.
- The sketch checks these flags and skips sleepy-only setup when missing.

## How to run

1. Flash `udp_sensor_collector` on one board.
2. Open Serial Monitor (`115200`) and wait for attached role + UDP bind output.
3. Flash `udp_sensor_node` on one or more boards.
4. Verify:
   - sensor prints TX + ACK,
   - collector prints RX lines and periodic fleet report.

## Operational notes

- Keep dataset constants aligned on both sketches.
- `MAX_SENSORS` on collector controls table capacity.
- `SAMPLE_PERIOD_MS` on sensor controls reporting rate.
- For large fleets, increase reporting interval and avoid synchronized bursts.

## Resilience and recovery

The transport is connectionless, so both ends watch liveness at the application
layer:

- **Sensor drops off (loses power):** the collector marks it `OFFLINE` after
  `NODE_OFFLINE_MS` of silence and frees its slot after `NODE_EVICT_MS`, so it
  stops being reported as present.
- **Collector reboots:** the dataset is persisted in NVS, so the collector
  resumes the *same* network (same mesh-local prefix and key state) instead of
  minting a new one - children that lost their parent re-attach to the returning
  leader within seconds. Its node table is RAM-only and is rebuilt automatically
  from the frames sensors keep sending; a role watchdog re-forms the network if
  the collector ever detaches.
- **Sensor stops getting ACKs:** after `REATTACH_AFTER_MISSED` consecutive
  misses (the signature of a vanished collector) the sensor forces a clean
  Thread re-attach and re-opens its UDP socket, so a Sleepy End Device does not
  sit polling a dead parent until `CHILD_TIMEOUT_S` expires.

## Troubleshooting quick checks

- `Error ...` after `udp open/bind/send`: verify Thread state and command order.
- No ACK on sensor:
  - collector not bound yet,
  - wrong port/group/dataset mismatch,
  - sensor detached (`state` not child/router/leader).
- Frequent drops on collector: malformed payload or table full.

## Why CLI-only

This example is intentionally independent from Native UDP API availability.
It can be merged and validated using existing OpenThread CLI functionality.

## License

Apache License 2.0.
