# ThreadScan_Discover — blocking Thread discovery (Native API)

Part of the [Thread Network Discovery demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadScan/README.md). This sketch:

* starts OpenThread with `begin(false)` (no NVS auto-load),
* brings up the IPv6 interface with `networkInterfaceUp()` (Thread start not required),
* calls `OThreadScan.discoverNetworks()` in blocking mode every 10 seconds,
* prints each `OThreadNetworkInfo` (network name, Extended PAN ID, PAN ID,
  extended address, channel, RSSI, LQI, joinable flag),
* frees results with `scanDelete()` after each scan.

It demonstrates the WiFiScan-style blocking pattern on Thread MLE discover
(`otThreadDiscover()` / CLI `discover`).

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF features (sdkconfig)

| Feature                             | Why                                    |
| ----------------------------------- | -------------------------------------- |
| `CONFIG_OPENTHREAD_ENABLED=y`       | Build the OpenThread stack.            |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Ensure the SoC has the 802.15.4 radio. |

## Prerequisites

[LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) must be running as **Leader** within RF range.

Flash the Leader first and wait for `Role: Leader`, then flash this sketch on
a second board.

## What the sketch does

```cpp
// 1) Stack + IPv6 interface only (no Thread start).
OThread.begin(false);
OThread.networkInterfaceUp();

// 2) Blocking discover every 10 s in loop().
int n = OThreadScan.discoverNetworks();
for (int i = 0; i < n; ++i) {
  const OThreadNetworkInfo &net = OThreadScan.getResult(i);
  // print net.networkName, net.extendedPanIdStr(), net.panId, ...
}
OThreadScan.scanDelete();
```

## Expected serial output

```text
Setup done — IPv6 interface up, Thread start not required
Thread discovery start
Thread discovery done
1 network(s) found
Nr | J | Network Name     | Extended PAN     | PAN  | MAC Address      | CH | dBm | LQI
 1 | 1 | ESP_OpenThread   | dead00beef00cafe | 1234 | aabbccddeeff0011 | 15 | -45 | 255
-------------------------------------
```

When no Leader is nearby:

```text
Thread discovery start
Thread discovery done
no Thread networks found
-------------------------------------
```

On failure:

```text
Thread discovery start
Thread discovery done
discovery failed (interface down, lock failure, or timeout)
-------------------------------------
```

If a previous scan is still running:

```text
Thread discovery start
Thread discovery done
discovery already in progress (OT_DISCOVER_RUNNING)
-------------------------------------
```

## Customization

### Stored result capacity

The library keeps at most ``OT_DISCOVER_MAX_RESULTS`` **unique** networks per scan
(default **16**). This sketch raises the limit to **32** for denser RF environments:

```cpp
#define OT_DISCOVER_MAX_RESULTS 32
#include "OThreadScan.h"
```

The ``#define`` must appear **before** ``#include "OThreadScan.h"`` (in your ``.ino``
or in a header included before it). You can also pass it from the build system, e.g.
``-DOT_DISCOVER_MAX_RESULTS=32``. Higher values use more RAM because result vectors
are pre-reserved to that size before each scan.

Discovery timeout defaults to 30 seconds in the library. Adjust before calling
`discoverNetworks()`:

```cpp
OThreadScan.setScanTimeout(60000);  // ms
```

Limit to one channel (11..26) with `OThreadScan.setChannel(15)`; `0` scans all
supported channels.

## Troubleshooting

**Startup order:** Start [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) first and wait for
`Role: Leader`. Then flash this sketch.

| Symptom | Likely cause |
| --- | --- |
| `no Thread networks found` | Leader not running or out of range — start Leader on another board. |
| `discovery failed` | Interface down, lock failure, or timeout — ensure `networkInterfaceUp()` was called; try a longer `setScanTimeout()`. |
| `discovery already in progress` | Previous scan still running (`OT_DISCOVER_RUNNING`) — wait for it to finish or call `scanDelete()` after completion. |
| Wrong network name / XPAN | Multiple Thread networks nearby — compare Extended PAN ID with the Leader. |
| No serial output | Serial Monitor not at **115200**. |

## See also

* [Thread Network Discovery — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadScan/README.md) — all three discovery patterns.
* [ThreadScan_Async](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Async) — non-blocking `scanComplete()` polling.
* [ThreadScan_Callback](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Callback) — per-network streaming callbacks.
* [CLI ThreadScan](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/ThreadScan) — CLI equivalent (`discover` command).

## License

Apache License 2.0.
