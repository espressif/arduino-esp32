# UDP Sensor Network over Thread - Native API

This folder contains a many-to-one telemetry example built with the Arduino
OpenThread Native API (`OThread` + `OThreadUDP`). The collector forms or resumes
a Thread network and accepts Joiners; each sensor resumes its stored dataset
after normal resets or joins with only the PSKd on first commissioning, then
periodically uploads a compact UDP reading.

| Sketch | Role |
| ------ | ---- |
| [`sensor_collector/sensor_collector.ino`](sensor_collector/) | Server: Thread **Leader + Commissioner** and high-node UDP sink. |
| [`sensor_node/sensor_node.ino`](sensor_node/) | Client: Thread **Joiner + optional Sleepy End Device** and periodic UDP sensor. |

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF Features (sdkconfig)

| Sketch | Requires |
| ------ | -------- |
| `sensor_collector` | `CONFIG_OPENTHREAD_ENABLED=y`, `CONFIG_SOC_IEEE802154_SUPPORTED=y`, `CONFIG_OPENTHREAD_COMMISSIONER=y` |
| `sensor_node` | `CONFIG_OPENTHREAD_ENABLED=y`, `CONFIG_SOC_IEEE802154_SUPPORTED=y`, `CONFIG_OPENTHREAD_JOINER=y` |

## Architecture

```text
sensor_node 1 ----\
sensor_node 2 -----+--> sensor_collector (Leader + Commissioner + UDP sink)
sensor_node N ----/
```

The collector listens on UDP port `5050`, tracks up to `MAX_SENSORS` unique
node IDs, marks silent nodes offline, evicts long-dead records, prints every
accepted sample, and replies with an application-level ACK:

```text
sensor_node -> sensor_collector:
id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>

sensor_collector -> sensor_node:
OK,<nodeId>,<seq>
```

Port `5050` is intentionally used for application traffic. Do not use `61631`
for this demo: Thread reserves that port for TMF CoAP, and binding an
application socket there can deliver binary management packets that look like
malformed sensor frames.

The collector is authoritative for sequence numbers. It ACKs the last sequence
it accepted for each node, so a sensor holds its sequence until an ACK arrives
and can resynchronise if the collector reports an older or newer stored value.

## How to Run

1. Flash [`sensor_collector/sensor_collector.ino`](sensor_collector/sensor_collector.ino)
   on one board and open Serial Monitor at `115200`.
2. Wait until it becomes attached, starts Commissioner, and prints
   `Collector listening on UDP port 5050`.
3. Flash [`sensor_node/sensor_node.ino`](sensor_node/sensor_node.ino) on one
   or more additional boards.
4. Confirm each sensor prints `status=ACKED` and the collector prints the
   incoming samples. If a sensor prints `NO_ACK`, it keeps the same sequence
   number for the next retry instead of advancing.

The collector and commissioned sensors persist their Thread datasets in NVS.
After power loss or reset they should reattach using the stored dataset. The
collector joiner window is needed again only for first-time sensors, erased NVS,
or changed network credentials.

## Customization

- In `sensor_collector/sensor_collector.ino`, tune `MAX_SENSORS`,
  `NODE_OFFLINE_MS`, `NODE_EVICT_MS`, `JOINER_WINDOW_SEC`, `COLLECTOR_PORT`,
  and the Thread dataset constants.
- In `sensor_node/sensor_node.ino`, tune `SAMPLE_PERIOD_MS`,
  `ENABLE_SLEEPY_END_DEV`, `SED_POLL_PERIOD_MS`, `CHILD_TIMEOUT_SEC`,
  `CHANNEL_HINT`, and `PSKD`.

## See Also

- [`../ThreadCommissioning/`](../ThreadCommissioning/) - Bare Joiner /
  Commissioner demo without UDP traffic.
- [`../UDP_Light_Switch/`](../UDP_Light_Switch/) - UDP light/switch control
  example using the same Native UDP socket class.

## License

Apache License 2.0.
