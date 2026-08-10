# sensor_node - Thread Joiner + Sleepy Sensor UDP Client

Client side of the [UDP Sensor Network demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/README.md). This sketch:

* **resumes** a stored Thread dataset from NVS after normal resets,
* runs the **Thread Joiner** state machine only when no stored dataset exists
  or the stored dataset cannot attach, using PSKd `J01NME`,
* optionally enables **Sleepy End Device** behavior (rx-off-when-idle + polls),
* sends one UDP telemetry frame per sample period to the collector Leader RLOC,
* waits for an application-level ACK and **holds** its sequence until confirmed,
* forces Thread re-attach after consecutive missed ACKs or prolonged detach.

It is the counterpart of [UDP Sensor Network — sensor_collector (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector).

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
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable `otJoiner*` APIs used for first join.     |

## Prerequisites

A [UDP Sensor Network — sensor_collector (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector) instance must already be running on
the same RF range with `Collector listening on UDP port 5050`.

For the sensor's **first commissioning** (or after erasing NVS), the collector
must have reached `Commissioner ACTIVE` and still be within its joiner window.

After the sensor has joined once, the dataset is stored in NVS. A later reset
or power loss can resume that stored dataset and reattach **without** the
collector reopening its joiner window.

`CHANNEL_HINT` and `PSKD` must match the collector configuration.

## What the sketch does

```cpp
// 1) Resume NVS dataset or run Joiner commissioning.
OThread.begin(false);
if (OThread.hasActiveDataset()) {
  OThread.networkInterfaceUp();
  OThread.start();
  // wait for attach...
} else {
  OThread.setChannel(CHANNEL_HINT);
  OThread.networkInterfaceUp();
  OThread.startJoiner("J01NME", JOIN_TIMEOUT_MS);
  OThread.start();
}

// 2) Optional SED: otLinkSetRxOnWhenIdle(false), poll period, child timeout.
// 3) Bind UDP, send frame, wait for OK,<nodeId>,<seq> ACK.
// 4) Advance sequence only on ACK; force reattach after REATTACH_AFTER_MISSED misses.
```

## Expected serial output

First commissioning:

```text
=== sensor_node: Thread Joiner + UDP sensor ===
No stored Thread dataset; commissioning is required.
Joining with PSKd "J01NME"...
Joiner: SUCCESS
Attached as Child.
TX try=1 [fdde:ad00:beef:0:....]:5050 -> 'id=3CAAB123,seq=1,temp_centi=2387,batt_mv=3810'
ACK [fdde:ad00:beef:0:....]:5050 <- 'OK,3CAAB123,1'
sample=1 temp=23.87C batt=3810mV status=ACKED
```

NVS resume after reset:

```text
=== sensor_node: Thread Joiner + UDP sensor ===
Stored Thread dataset found; resuming network without commissioning.
.....
Attached as Child.
TX try=1 [fdde:ad00:beef:0:....]:5050 -> 'id=3CAAB123,seq=2,...'
ACK ... status=ACKED
```

If no valid ACK:

```text
sample=3 temp=24.01C batt=3795mV status=NO_ACK (sequence held)
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant                  | Purpose                                           |
| ------------------------- | ------------------------------------------------- |
| `PSKD`                    | Pre-Shared Key for Device. Must match collector.  |
| `CHANNEL_HINT`            | Joiner channel hint (default 15).                 |
| `COLLECTOR_PORT`          | UDP port (default 5050).                          |
| `JOIN_TIMEOUT_MS`         | Max wait inside `startJoiner()`.                  |
| `RESUME_ATTACH_TIMEOUT_MS`| Max wait for NVS resume before retrying Joiner. |
| `SAMPLE_PERIOD_MS`        | Report cadence (default 30000 ms).                |
| `ACK_TIMEOUT_MS`          | Per-attempt ACK wait.                             |
| `TX_RETRIES`              | Retries per sample before giving up.              |
| `REATTACH_AFTER_MISSED`   | Consecutive `NO_ACK` samples before re-attach.    |
| `DETACHED_REATTACH_MS`    | Force re-attach if detached this long.            |
| `ENABLE_SLEEPY_END_DEV`   | Enable SED behavior (default true).               |
| `SED_POLL_PERIOD_MS`      | Child data poll period.                           |
| `CHILD_TIMEOUT_SEC`       | Parent child timeout.                             |

Node ID is derived from factory EUI-64 into a fixed char buffer (no `String`
heap churn). Replace `readFakeSensor()` with your hardware sensor read code.

This example demonstrates low-power Thread behavior without MCU deep sleep. If
you use deep sleep, the device must reinitialize and reattach at wake.

## Troubleshooting

**Startup order:** Start [UDP Sensor Network — sensor_collector (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector) first and
wait until it is Leader with Commissioner active and UDP bound on port 5050.
Then flash this sketch. Reset this board if it booted before the collector was
ready.

| Symptom | Likely cause |
| --- | --- |
| Joiner fails on first boot | Collector not running or join window closed — start collector first, then reset this node. |
| `Stored Thread dataset found; resuming...` then `NO_ACK` | Collector down or unreachable — node retries and may force re-attach. |
| `NO_ACK (sequence held)` | Collector not receiving or ACKing — verify matching dataset/PSKd and port 5050. |
| Sleepy node slow to recover after collector reboot | Parent loss until child timeout — sketch forces re-attach after `REATTACH_AFTER_MISSED`. |
| Changed `PSKD` / `CHANNEL_HINT` ignored | Stored NVS dataset takes precedence — erase NVS for fresh Joiner commissioning. |

## See also

* [UDP Sensor Network — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/README.md) — wire protocol and multi-node architecture.
* [UDP Sensor Network — sensor_collector (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector) — companion Leader + UDP telemetry sink.
* [Thread Commissioning — JoinerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) — same Joiner flow without UDP traffic.

## License

Apache License 2.0.
