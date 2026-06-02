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
udp bind :: 61631
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

ACK expected:

```text
OK,<nodeId>,<seq>
```

## Expected serial output

```text
=== udp_sensor_node (CLI): Sleepy UDP sensor ===
...
Role: Child
Node ID: 3CAAB123
Collector group: ff03::abcd:61631
TX [ff03::abcd]:61631 -> 'id=3CAAB123,seq=1,temp_centi=2312,batt_mv=3820'
ACK [fd..]:61631 <- 'OK,3CAAB123,1'
sample=1 temp=23.12C batt=3820mV status=ACKED
```

## Customization

Tune these constants in `udp_sensor_node.ino`:

- `SAMPLE_PERIOD_MS`
- `USE_SLEEPY_MODE`
- `SED_POLL_MS`
- `CHILD_TIMEOUT_S`

## License

Apache License 2.0.
