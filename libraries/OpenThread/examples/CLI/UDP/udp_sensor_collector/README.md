# OpenThread UDP Sensor Collector (CLI)

This sketch is the collector/leader side of the CLI-based UDP sensor demo.

It:

- configures Thread with CLI commands,
- starts and attaches as a Thread node (Leader in a fresh partition),
- binds a UDP socket on port `5050`,
- receives sensor frames from many nodes,
- prints decoded values and periodic fleet status to Serial,
- replies with per-frame ACK.

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

## CLI setup sequence used

On the **first** boot (no dataset stored in NVS) the sketch provisions the
network. The mesh-local prefix and active timestamp are pinned so the network is
identical every time it is created:

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
udp close
udp open
ipmaddr add ff03::abcd
udp bind :: 5050
```

On **subsequent** boots the sketch detects the persisted dataset with the
Arduino `OThread.hasActiveDataset()` API and resumes the same network with just:

```text
ifconfig up
thread start
```

This is what lets children rejoin quickly after a leader reset: the collector
returns as the same network rather than a new one. To apply changed `OT_*`
constants you must clear the stored dataset first (CLI `factoryreset`).

This is the CLI equivalent of the Native `DataSet` setup path.
UDP RX/TX is also CLI-only (no `OThreadUDP` object).

## Wire protocol

Incoming telemetry payload:

```text
id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
```

ACK payload (the collector's authoritative last-stored sequence for that node,
which the node uses to confirm delivery and resync its own counter):

```text
OK,<nodeId>,<collectorLastSeq>
```

### Sequence handling

The collector owns each node's sequence and classifies every incoming frame:

| Incoming `seq` vs stored `lastSeq` | Meaning | Action | RX note |
| --- | --- | --- | --- |
| first frame / `seq > lastSeq` | new reading | store reading, `lastSeq = seq` | — |
| `seq == lastSeq` | node missed our ACK | count duplicate, keep reading | `(dup)` |
| `seq == 1` while `lastSeq > 1` | node restarted | follow it: `lastSeq = 1`, store reading | `(restart)` |
| other `seq < lastSeq` | old/out-of-order frame | ignore reading | `(stale)` |

In every case the ACK carries the **stored** `lastSeq`, so a node that resent a
missed frame is confirmed, and a node that sent a stale frame is told the real
last sequence and resyncs forward.

## Expected serial output

```text
=== udp_sensor_collector (CLI): Leader + UDP sink ===
...
Attached as Leader
Collector listening on UDP ff03::abcd:5050
node 3CAAB123 -> ONLINE
RX node=3CAAB123 seq=1 temp=23.18C batt=3810mV from [fd..]:5050
[collector-cli] role=Leader nodes=1 online=1 packets=1 dropped=0
node 3CAAB123 -> OFFLINE (silent 96s)
```

## Scaling notes

- In-memory table tracks up to `MAX_SENSORS` (`256`) unique sensor IDs.
- Periodic report shows `nodes` (known) and `online` (heard from recently).
- If table fills up, new unknown IDs are dropped and counted.

## Liveness and recovery

- Each node is flipped to `OFFLINE` after `NODE_OFFLINE_MS` of silence (a few
  missed reports) and its slot is freed after `NODE_EVICT_MS`, so a sensor that
  loses power stops being reported as present.
- The node table lives only in RAM. After a collector reset the table starts
  empty and is rebuilt automatically from the frames sensors keep multicasting -
  no persisted state is needed to recover.
- A role watchdog re-forms the network if the collector ever detaches.

## Troubleshooting

### `DROP malformed` from RLOC addresses (e.g. `...:0:ff:fe00:f801`)

If the collector logs a stream of `DROP malformed` packets whose source is an
RLOC address (IID `0000:00ff:fe00:<rloc16>`) and whose payload is binary
starting with `0x42` ('B'), the application UDP port collides with a Thread
**reserved port** and the socket is receiving Thread's own management traffic
(the `0x42` byte is a CoAP `Ver=1, CON, TKL=2` header). Do **not** bind these:

| Port | Used by Thread for |
| --- | --- |
| `61631` | Thread Management Framework (TMF) CoAP (address query/notify, network data) |
| `5683` / `5684` | CoAP / CoAPs |
| `19788` | MLE |

This example uses `5050`, a free application port. Pick any non-reserved port
and keep it identical on the collector and the nodes.

## License

Apache License 2.0.
