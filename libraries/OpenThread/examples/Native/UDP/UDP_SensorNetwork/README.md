# UDP Sensor Network over Thread — Native API

Many-to-one telemetry example built with the Arduino OpenThread Native API
(`OThread` + `OThreadUDP`). The collector forms or resumes a Thread network and
accepts Joiners; each sensor resumes its stored dataset after normal resets or
joins with only the PSKd on first commissioning, then periodically uploads a
compact UDP reading with application-level sequence ACKs.

| Sketch | Role |
| ------ | ---- |
| [sensor_collector (Leader + UDP sink)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector) | Server: Thread **Leader + Commissioner** and high-node UDP sink on port **5050**. |
| [sensor_node (Joiner sensor client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) | Client: Thread **Joiner** (+ optional **Sleepy End Device**) and periodic UDP sensor. |

```text
sensor_node 1 ----\
sensor_node 2 -----+--> sensor_collector (Leader + Commissioner + UDP sink)
sensor_node N ----/
```

## Wire protocol

Sensor uplink (ASCII):

```text
id=<nodeId>,seq=<u32>,temp_centi=<i32>,batt_mv=<u16>
```

Collector ACK:

```text
OK,<nodeId>,<seq>
```

Port **5050** is used intentionally for application traffic. Do not use **61631**
(Thread TMF CoAP) or **5683**/**5684** (CoAP) for this demo.

The collector is authoritative for sequence numbers. Sensors hold their sequence
until an ACK arrives and can resynchronise from the collector's stored value.

## How to Run

1. Flash [sensor_collector (Leader + UDP sink)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector)
   on one board and open Serial Monitor at `115200`.
2. Wait until it prints `Collector listening on UDP port 5050`.
3. Flash [sensor_node (Joiner sensor client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) on one or
   more additional boards.
4. Confirm each sensor prints `status=ACKED` and the collector prints incoming
   samples.

The collector and commissioned sensors persist their Thread datasets in NVS.
After power loss they should reattach without reopening the joiner window.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `sensor_collector` |
| `CONFIG_OPENTHREAD_JOINER=y` | `sensor_node` |

## Troubleshooting

**Startup order:** Flash [UDP Sensor Network — sensor_collector (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector) first and wait
for `Collector listening on UDP port 5050`. Then flash one or more
[UDP Sensor Network — sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) boards. Reset any sensor that booted before the
collector was ready.

| Symptom | Likely cause |
| --- | --- |
| Sensor cannot join (first boot) | Collector not running or Commissioner join window closed — start collector first, then reset sensor. |
| Sensor `NO_ACK` | Collector not bound, dataset/PSKd mismatch, or wrong port — verify collector Serial and matching `CHANNEL_HINT` / `PSKD`. |
| Collector reset, sensors stuck | Sensors force re-attach after missed ACKs — wait or reset sensors if needed. |
| Sensor works after power loss, not first join | Expected — commissioned sensors resume NVS dataset; joiner window only for first commission or erased NVS. |
| `DROP malformed` on collector | Application bound to reserved port — use **5050**, not 61631/5683/5684. |

## See Also

* [Native UDP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/README.md) — all Native UDP demos and port conventions.
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — Joiner / Commissioner without UDP.
* [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) — UDP light/switch on port 5051.
* [CoAP Sensor](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor) — CoAP telemetry on port 5683.

## License

Apache License 2.0.
