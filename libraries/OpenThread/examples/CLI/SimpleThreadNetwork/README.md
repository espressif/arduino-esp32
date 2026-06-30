# Simple Thread Network — CLI

A minimal Thread network brought up through the **OpenThread CLI** (textual
`dataset` / `ifconfig` / `thread start` commands sent via `OThreadCLI` and the
helper functions in `OThreadCLI_Util`). One board forms the network as Leader;
the others join with the matching network key and channel.

| Sketch | Role |
| --- | --- |
| [CLI LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/LeaderNode) | Forms a new network and becomes Leader via CLI commands |
| [CLI RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/RouterNode) | Joins the network (Router/Child) via CLI commands |
| [CLI ExtendedRouterNode (CLI + Native mix)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/ExtendedRouterNode) | Same as RouterNode, but also shows reading the same value through **both** the CLI helpers and the native OpenThread API |

All three use `OThread.begin(false)` followed by `OThreadCLI.begin()`, then drive
provisioning with CLI commands. This is the CLI counterpart of the typed
[Simple Thread Network (Native API)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork) example.

## How to Run

1. Flash [CLI LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/LeaderNode) on one ESP32-H2 / ESP32-C6 / ESP32-C5 board
   and wait until it reports the Leader role.
2. Flash [CLI RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/RouterNode) or [CLI ExtendedRouterNode (CLI + Native mix)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/ExtendedRouterNode) on a second board
   using the **same network key and channel**.
3. The second board joins as `Router` (or `Child`) and prints network
   information.

Each sketch has its own README with configuration, expected output, and the CLI
command sequence it issues.

## Troubleshooting

**Startup order:** Flash [CLI LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/LeaderNode) first and wait for `Role: Leader`. Then flash [CLI RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/RouterNode) or [CLI ExtendedRouterNode (CLI + Native mix)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/ExtendedRouterNode) with the **same network key and channel**. Reset the joiner board if it booted before the Leader was ready.

| Symptom | Likely cause |
| --- | --- |
| Router stuck as Detached | Leader not running yet — start Leader first, then reset router. |
| Network key or channel mismatch | `CLI_NETWORK_KEY` and `CLI_NETWORK_CHANNEL` must match Leader exactly. |
| ExtendedRouterNode setup timeout (90 s) | Leader down, wrong credentials, or out of RF range — verify Leader Serial, then reset router. |
| No serial output | Serial Monitor not at **115200** or wrong USB port. |

## See Also

- [Simple Thread Network (Native API)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork) — the same scenario with the typed Native API.
- [CLI examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/CLI/README.md) — CLI vs Native overview and mixed-usage guidance.

## License

Apache License 2.0.
