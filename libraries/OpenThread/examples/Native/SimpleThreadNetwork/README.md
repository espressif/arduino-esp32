# Simple Thread Network — Native API

Minimal two-board Thread network built with the typed **Native API**
(`OpenThread` + `DataSet` classes, no CLI strings). One board forms the network
and becomes Leader; the other joins it using only the shared **network key**.

| Sketch | Role |
| --- | --- |
| [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) | Forms a new network from a full `DataSet` and becomes Leader |
| [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode) | Joins the network with the matching network key (Router/Child) |

Both sketches use `OThread.begin(false)` (no NVS auto-start) so the dataset is
configured explicitly in the sketch. The Router node only needs the **network
key** to attach — the remaining parameters are learned from the Leader.

## How to Run

1. Flash [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) on one ESP32-H2 / ESP32-C6 /
   ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait until Serial reports `Role: Leader`.
3. Flash [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode) on a second board using the
   **same network key**.
4. Within a few seconds the second board attaches as `Router` (or `Child`) and
   both print live network information every 5 seconds.

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |

## Troubleshooting

**Startup order:** Flash [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) first and wait for
`Role: Leader`. Then flash [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode) with the **same network
key**. If the router booted before the Leader was ready, press **reset** on the
router after the Leader is up.

| Symptom | Likely cause |
| --- | --- |
| Router stuck as Detached | Leader not running yet or network key mismatch — start Leader first, then reset router. |
| Router on wrong network | Stale NVS on either board — both use `begin(false)` but erase flash if a prior dataset interferes. |
| Leader not becoming Leader | Another Leader on same channel/PAN — power off other Thread devices or change dataset constants. |
| No serial output | Serial Monitor not at **115200** or wrong USB port. |

## See Also

* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — join a network securely with a PSKd instead of sharing the network key.
* [CLI Simple Thread Network examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork) — the same scenario driven through the OpenThread CLI.
* [Native examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md) — all Native API example categories.

## License

Apache License 2.0.
