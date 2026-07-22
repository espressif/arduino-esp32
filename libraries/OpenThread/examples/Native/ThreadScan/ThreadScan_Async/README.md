# ThreadScan_Async — async Thread discovery (Native API)

Part of the [Thread Network Discovery demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadScan/README.md). This sketch:

* starts OpenThread with `begin(false)` and `networkInterfaceUp()`,
* starts discovery with `OThreadScan.discoverNetworks(true)` (returns
  `OT_DISCOVER_RUNNING` immediately),
* polls `OThreadScan.scanComplete()` from `loop()` while doing other work,
* prints `OThreadNetworkInfo` results when the scan finishes,
* restarts discovery automatically after each cycle.

It demonstrates the WiFiScanAsync-style non-blocking pattern on Thread MLE
discover.

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
// 1) Stack + IPv6 interface.
OThread.begin(false);
OThread.networkInterfaceUp();

// 2) Start async discovery (returns OT_DISCOVER_RUNNING).
OThreadScan.setScanTimeout(30000);
OThreadScan.discoverNetworks(true);

// 3) Poll in loop() while other work runs.
int16_t status = OThreadScan.scanComplete();
if (status >= 0) {
  const OThreadNetworkInfo &net = OThreadScan.getResult(0);
  OThreadScan.scanDelete();
}
```

## Expected serial output

```text
Setup done — starting async discovery
Thread discovery async start
loop running...
loop running...
async discovery done
1 network(s):
  [0] ESP_OpenThread | extPan=dead00beef00cafe | pan=1234 | aabbccddeeff0011 | ch=15 | -45 dBm | lqi=255 | joinable=yes
Thread discovery async start
loop running...
```

When discovery fails to start or times out:

```text
discovery failed to start
async discovery failed — restarting
```

## Customization

| Call | Purpose |
| --- | --- |
| `OThreadScan.setScanTimeout(30000)` | Overall timeout in milliseconds (set in `startAsyncDiscover()`). |
| `OThreadScan.setChannel(15)` | Scan a single channel; omit or pass `0` for all channels. |

## Troubleshooting

**Startup order:** Start [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) first and wait for
`Role: Leader`. Then flash this sketch.

| Symptom | Likely cause |
| --- | --- |
| Stuck on `loop running...` | Scan still in progress — normal until timeout or completion. |
| `async discovery failed` | Leader not nearby or timeout — increase `setScanTimeout()`. |
| `discovery failed to start` | Scan already in progress or interface down. |
| No networks in results | Leader not running — start Leader on another board. |

## See also

* [Thread Network Discovery — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadScan/README.md) — all three discovery patterns.
* [ThreadScan_Discover](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Discover) — blocking `discoverNetworks()`.
* [ThreadScan_Callback](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Callback) — per-network streaming callbacks.
* [Wi-Fi WiFiScanAsync](https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFi/examples/WiFiScanAsync) — same async polling pattern on Wi-Fi.

## License

Apache License 2.0.
