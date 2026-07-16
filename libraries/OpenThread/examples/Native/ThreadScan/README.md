# Thread Network Discovery — Native API

Sketches that discover nearby Thread networks using the typed **Native API**
(`OThreadScan` + `OThreadNetworkInfo`, no CLI strings). Each sketch calls
`OThreadScan.discoverNetworks()`, which wraps `otThreadDiscover()` (OpenThread
CLI `discover`).

Results include Thread identity (network name, Extended PAN ID, joinable flag)
and IEEE 802.15.4 link fields (extended address, PAN ID, channel, RSSI, LQI) —
the same primitive Matter uses to list Thread networks during commissioning.

| Sketch | Pattern |
| --- | --- |
| [ThreadScan_Discover](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Discover) | Blocking discovery (`discoverNetworks()`) |
| [ThreadScan_Async](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Async) | Non-blocking discovery with `scanComplete()` polling |
| [ThreadScan_Callback](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Callback) | Streaming `onResult()` / `onComplete()` callbacks |

> **Note:** These examples demonstrate the `OThreadScan` API provided by the library.

Thread does **not** need to be started for discovery — only
`OThread.networkInterfaceUp()` is required after `OThread.begin(false)`.

**Result access:** use `getResult()` / `getResultCount()` only after discovery
completes (`discoverNetworks()` ≥ 0, `scanComplete()` ≥ 0, or `onComplete()`).
While a scan is running, use `onResult()` for streaming.
Do not call `scanDelete()` (or other `OThreadScan` methods) from inside
`onResult()` / `onComplete()` — free results from `loop()` after
`scanComplete()` finishes.

**Stored result cap:** default `OT_DISCOVER_MAX_RESULTS` is **16** unique networks.
Override with `#define OT_DISCOVER_MAX_RESULTS 32` before `#include "OThreadScan.h"`
(see `ThreadScan_Discover` example).

## How to Run

1. Flash [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) on one ESP32-H2 / ESP32-C6 /
   ESP32-C5 board and open Serial Monitor at **115200**.
2. Wait until Serial reports `Role: Leader`.
3. Flash any ThreadScan sketch on a second board.
4. Open Serial Monitor at **115200** and confirm discovery output within one
   scan cycle (10 seconds for Discover/Callback; Async prints continuously).

Each sketch folder has its own README with expected output and troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | All ThreadScan sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | All ThreadScan sketches |

## Troubleshooting

**Startup order:** A Thread Leader or Router must be active nearby before
discovery returns useful results. Flash
[LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) first and wait for
`Role: Leader`, then run a ThreadScan sketch on a second board.

| Symptom | Likely cause |
| --- | --- |
| No networks found | Leader not running, out of RF range, or on a channel not covered by the scan — start Leader first. |
| `discovery failed` | IPv6 interface not up — call `OThread.networkInterfaceUp()` before `discoverNetworks()`. |
| Async scan never completes | Timeout too short — increase `OThreadScan.setScanTimeout()` (default 30 s). |
| No serial output | Serial Monitor not at **115200** or wrong USB port. |

## See also

* [Native examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md) — all Native API example categories.
* [CLI ThreadScan](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/ThreadScan) — same operations via `OThreadCLI` (`scan` / `discover`).
* [Simple Thread Network — LeaderNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) — companion network former for multi-board tests.
* [WiFi WiFiScan](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/examples/WiFiScan) — similar blocking scan ergonomics on Wi-Fi.

## License

Apache License 2.0.
