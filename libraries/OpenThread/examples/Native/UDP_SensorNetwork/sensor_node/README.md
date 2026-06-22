# sensor_node - Joiner + Sleepy Sensor UDP Client

Sensor side of the large-scale Thread UDP collector demo under `Native/UDP_SensorNetwork`.

This sketch:

- resumes a stored Thread dataset from NVS after normal resets,
- joins the Thread network using Joiner/Commissioner flow (`PSKd`) only when no
  stored dataset exists or the stored dataset cannot attach,
- optionally enables Sleepy End Device behavior,
- reads a simulated sensor value,
- sends one UDP frame per sample period to the collector (Leader RLOC),
- waits for a UDP ACK and prints status. The application sequence only advances
  after the collector ACKs it.

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF features (sdkconfig)

| Feature                              | Why                           |
| ------------------------------------ | ----------------------------- |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build OpenThread stack        |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure IEEE 802.15.4 support  |
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable Joiner APIs            |

## Sleepy node behavior

When `ENABLE_SLEEPY_END_DEV` is `true`, the sketch sets:

- `otLinkSetRxOnWhenIdle(..., false)` (sleepy child mode),
- `otLinkSetPollPeriod(..., SED_POLL_PERIOD_MS)` (data poll interval),
- `otThreadSetChildTimeout(..., CHILD_TIMEOUT_SEC)` (parent retention time).

This demonstrates OpenThread keeping sleepy children attached while they only
wake periodically to push data.

## CLI/COP mapping

| Sketch call                    | CLI/COP equivalent                        |
| ----------------------------- | ----------------------------------------- |
| `OThread.networkInterfaceUp()`| `ifconfig up`                             |
| `OThread.startJoiner(PSKD)`   | `joiner start <PSKd>`                     |
| `OThread.start()`             | `thread start`                            |
| `otLinkSetRxOnWhenIdle(...0)` | `mode n` (no rx-on-when-idle bit)         |
| `otLinkSetPollPeriod(ms)`     | `pollperiod <ms>`                         |
| `otThreadSetChildTimeout(s)`  | `childtimeout <seconds>`                  |

## Running this sketch

1. Start `sensor_collector/sensor_collector.ino` first on another board.
2. Flash this sketch and open serial monitor at `115200`.
3. On first commissioning, confirm Joiner succeeds. On later resets, confirm the
   node resumes the stored dataset without requiring the collector joiner window.
4. Confirm periodic lines appear:

```text
Stored Thread dataset found; resuming network without commissioning.
.....
Attached as Child.
TX [fd..leader..]:5050 -> 'id=...,seq=1,temp_centi=...,batt_mv=...'
ACK [fd..leader..]:5050 <- 'OK,...,1'
sample=1 temp=23.12C batt=3820mV status=ACKED
```

If no valid ACK arrives, the node prints `NO_ACK`, keeps the previous confirmed
sequence number, and retries the same next sequence on the next sample. After
several consecutive misses it forces a Thread re-attach so it can recover from a
collector reset or parent loss.

## Customization

Main knobs at top of `sensor_node/sensor_node.ino`:

- `SAMPLE_PERIOD_MS`: report cadence per sensor.
- `ACK_TIMEOUT_MS`, `TX_RETRIES`, `REATTACH_AFTER_MISSED`: application ACK and
  recovery behavior.
- `RESUME_ATTACH_TIMEOUT_MS`: max wait for reattach using a stored NVS dataset
  before retrying Joiner commissioning.
- `ENABLE_SLEEPY_END_DEV`: enable or disable SED behavior.
- `SED_POLL_PERIOD_MS`: child data poll period.
- `CHANNEL_HINT` and `PSKD`: must match collector configuration.

## Notes

- Sensor read is simulated by default (`readFakeSensor()`); replace with your
  hardware-specific sensor and battery read code.
- Node ID is derived from the factory EUI-64 into a fixed char buffer, avoiding
  heap churn while still keeping IDs stable across resets.
- A commissioned node stores its Thread dataset in NVS. Power-loss resets should
  reattach from that dataset even if the collector is no longer accepting
  joiners. Reopen the collector joiner window only for first commissioning,
  erased NVS, or changed network credentials.
- This example demonstrates low-power Thread behavior without MCU deep sleep.
  If you use deep sleep, the device must reinitialize and reattach at wake.

## License

Apache License 2.0.
