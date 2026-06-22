# sensor_collector - Thread Leader + UDP Sensor Collector

Collector side of the large-scale sensor demo under `Native/UDP_SensorNetwork`.

This sketch:

- creates a Thread network with a fixed dataset,
- resumes the persisted Active Dataset from NVS on reboot instead of creating a
  random new partition,
- petitions Commissioner role and opens a PSKd join window,
- listens on UDP port `5050`,
- receives sensor frames from many end nodes and prints every sample to serial,
- tracks up to `MAX_SENSORS` unique node IDs in memory and periodically prints
  compact fleet statistics.
- marks nodes `OFFLINE` after `NODE_OFFLINE_MS` of silence and evicts records
  after `NODE_EVICT_MS` so dead nodes do not stay counted forever.
- restarts Thread and re-opens the UDP socket if the collector ever detaches.

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF features (sdkconfig)

| Feature                              | Why                                |
| ------------------------------------ | ---------------------------------- |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build OpenThread stack             |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure IEEE 802.15.4 radio exists  |
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable Commissioner APIs           |

## Wire Protocol

Incoming UDP payload (ASCII):

```text
id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
```

Example:

```text
id=3CAAB123,seq=42,temp_centi=2387,batt_mv=3810
```

Collector ACK:

```text
OK,<nodeId>,<seq>
```

The ACK sequence is the collector's stored sequence for that node. Duplicate or
stale frames are acknowledged with the stored value so the sensor can
resynchronise and avoid advancing without confirmed delivery.

> **Port note:** this example uses UDP port `5050`. Avoid `61631` for
> application traffic because Thread uses it for TMF CoAP; binding there can
> make the collector receive binary management traffic and report malformed
> packets.

## CLI/COP mapping

The sketch uses Native API calls that map to OpenThread CLI/COP concepts:

| Sketch call                           | CLI/COP equivalent                                    |
| ------------------------------------- | ----------------------------------------------------- |
| `OThread.networkInterfaceUp()`        | `ifconfig up`                                         |
| `OThread.start()`                     | `thread start`                                        |
| `OThread.startCommissioner()`         | `commissioner start`                                  |
| `OThread.addJoiner(PSKD, nullptr, t)` | `commissioner joiner add * <PSKd> <timeoutSec>`       |
| `OThread.otGetDeviceRole()`           | `state`                                               |

## How to run

1. Flash this sketch on one board and open serial monitor at `115200`.
2. Wait until you see:
   - `Attached as Leader` (or Router/Child in an existing partition),
   - `Commissioner ACTIVE...`,
   - `Collector listening on UDP port 5050`.
3. Flash `sensor_node/sensor_node.ino` on additional boards.
4. Confirm each sample arrives and gets ACKed.

On first boot the collector provisions a new Active Dataset. On later boots it
resumes the dataset stored in NVS, so resetting the collector brings back the
same Thread network instead of forcing every sensor through a fresh partition.
Sensor nodes that have already been commissioned also resume their stored NVS
dataset, so they do not require the collector joiner window after a normal
power-loss reset.

## Scaling Notes

- `MAX_SENSORS` is set to `256` records in RAM.
- Report line every 30 seconds shows:
  - known nodes,
  - online nodes,
  - total packets and dropped packets.
- `NODE_OFFLINE_MS` controls when a silent node is reported offline.
- `NODE_EVICT_MS` controls when a silent node is removed from the table.
- Real "couple hundred" deployments also require RF planning and realistic
  reporting interval so the mesh is not saturated by uplink bursts.

## License

Apache License 2.0.
