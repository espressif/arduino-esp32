# sensor_collector - Thread Leader + Commissioner + UDP Sensor Collector

Server side of the [UDP Sensor Network demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/README.md). This sketch:

* forms or **resumes** a Thread network `ESP_OT_SENSOR_NET` with a fixed
  `DataSet` (NVS resume on reboot),
* petitions the **Commissioner** role and opens a joiner window for PSKd
  `J01NME`,
* listens on UDP port **5050** for sensor telemetry frames,
* tracks up to `MAX_SENSORS` node IDs, marks silent nodes offline, evicts stale
  records, and sends application-level sequence ACKs,
* restarts Thread and re-opens the UDP socket if the collector ever detaches.

It is the counterpart of [UDP Sensor Network — sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node).
One collector can serve many sensor nodes on the same network.

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
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable Commissioner APIs for Joiner admission.   |

## Prerequisites

Flash this sketch **before** any [UDP Sensor Network — sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) boards.

Wait for `Commissioner ACTIVE` and `Collector listening on UDP port 5050`
before flashing or resetting sensors.

On first boot the collector provisions a new Active Dataset. On later boots it
resumes the dataset stored in NVS, so resetting the collector brings back the
same Thread network. Commissioned sensors also resume NVS and do not need the
joiner window after a normal power-loss reset.

## What the sketch does

```cpp
// 1) Resume NVS dataset or provision a new one, then start Thread.
if (OThread.hasActiveDataset()) {
  OThread.networkInterfaceUp();
  OThread.start();
} else {
  OThread.commitDataSet(ds);
  OThread.networkInterfaceUp();
  OThread.start();
}

// 2) Petition Commissioner and open the joiner window.
OThread.startCommissioner();
OThread.addJoiner("J01NME", JOINER_WINDOW_SEC);

// 3) Bind UDP on port 5050 and process incoming frames in loop().
OtUdp.begin(COLLECTOR_PORT);
// parse id=...,seq=..., reply OK,<nodeId>,<seq>
// role watchdog restarts Thread/UDP if detached
```

## Wire protocol

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
stale frames are acknowledged with the stored value so sensors can resynchronise.

> **Port note:** use UDP port **5050**. Avoid **61631** (Thread TMF CoAP) and
> **5683**/**5684** (CoAP) for application traffic.

## Expected serial output

```text
=== sensor_collector: Thread Leader + UDP sink ===
Attached as Leader.
Commissioner ACTIVE. PSKd "J01NME" open for 3600 s.
Collector listening on UDP port 5050
RX [fdde:ad00:beef:0:....]:5050 <- 'id=3CAAB123,seq=1,temp_centi=2387,batt_mv=3810'
TX ACK -> 'OK,3CAAB123,1'
Fleet: known=1 online=1 total_rx=1 dropped=0
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Joiner secret accepted by the Commissioner.                  |
| `JOINER_WINDOW_SEC`| How long `addJoiner()` stays valid (default 3600 s).           |
| `OT_CHANNEL`       | 802.15.4 channel (default 15).                               |
| `OT_PAN_ID`        | 16-bit PAN ID (default `0xABCE`).                            |
| `OT_EXTPANID`      | Extended PAN ID bytes.                                       |
| `OT_NETKEY`        | 128-bit network key.                                         |
| `OT_NETWORK_NAME`  | Thread network name string.                                  |
| `COLLECTOR_PORT`   | UDP listen port (default 5050).                              |
| `MAX_SENSORS`      | In-memory node table size (default 256).                     |
| `NODE_OFFLINE_MS`  | Silence before a node is marked offline.                     |
| `NODE_EVICT_MS`    | Silence before a node record is evicted.                     |
| `REPORT_PERIOD_MS` | Fleet statistics print interval.                             |

If changed dataset constants have no effect, erase NVS or run OpenThread
`factoryreset` — the persisted Active Dataset takes precedence.

## Troubleshooting

**Startup order:** Flash this collector first. Wait for `Collector listening on
UDP port 5050`, then flash [UDP Sensor Network — sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) boards. Reset any
sensor that booted before the collector was ready.

| Symptom | Likely cause |
| --- | --- |
| No sensors attaching (first commission) | Commissioner join window not open — wait for `Commissioner ACTIVE` before starting sensors. |
| Collector reset loses node table | Expected — table is RAM-only; sensors rebuild it after collector reattaches from NVS. |
| `DROP malformed` from RLOC addresses | Bound to Thread-reserved port — use **5050**, not 61631/5683/5684. |
| Changed `OT_*` constants have no effect | Dataset persisted in NVS — erase flash or `factoryreset` before expecting new constants. |
| Collector detaches | Role watchdog restarts Thread and reopens UDP socket — check RF and build flags. |

## See also

* [UDP Sensor Network — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/README.md) — wire protocol and multi-node architecture.
* [UDP Sensor Network — sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) — Joiner + optional SED sensor client.
* [Thread Commissioning — CommissionerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) — same Leader + Commissioner without UDP traffic.

## License

Apache License 2.0.
