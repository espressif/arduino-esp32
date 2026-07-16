# ThreadScan_Callback — streaming Thread discovery (Native API)

Part of the [Thread Network Discovery demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadScan/README.md). This sketch:

* starts OpenThread with `begin(false)` and `networkInterfaceUp()`,
* registers `OThreadScan.onResult()` to print each network as it is discovered,
* registers `OThreadScan.onComplete()` for the final summary,
* starts non-blocking discovery with `discoverNetworks(true)` and waits for
  completion in `loop()` every 10 seconds.

This matches the OpenThread and Matter delivery model: Discovery Responses
stream in through a callback rather than as a single batch at the end.

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
// 1) Register streaming callbacks.
OThreadScan.onResult(onDiscoverResult);
OThreadScan.onComplete(onDiscoverComplete);
OThreadScan.setScanTimeout(30000);

// 2) Non-blocking start; wait for completion, then free results from loop().
OThreadScan.discoverNetworks(true);
while (OThreadScan.scanComplete() == OT_DISCOVER_RUNNING) {
  delay(50);
}
OThreadScan.scanDelete();
```

`onDiscoverResult` prints each `OThreadNetworkInfo` as OpenThread receives it.
`onDiscoverComplete` prints the final summary only — call `scanDelete()` from
`loop()` after `scanComplete()` indicates completion (not from the callback).

## Expected serial output

```text
Setup done — callback discovery
Thread discovery start (callbacks)
  found: ESP_OpenThread | extPan=dead00beef00cafe | pan=1234 | ch=15 | -45 dBm | joinable=yes
discovery complete: 1 network(s) (1 reported live)
```

When no networks are found:

```text
Thread discovery start (callbacks)
discovery complete: 0 network(s) (0 reported live)
```

On start failure:

```text
Thread discovery start (callbacks)
discovery failed to start
```

## Customization

Register different callbacks in `setup()`:

```cpp
OThreadScan.onResult(myResultHandler, myContext);
OThreadScan.onComplete(myCompleteHandler, myContext);
```

Optional discover filters (before `discoverNetworks()`):

```cpp
OThreadDiscoverFilters filters;
filters.joinerOnly = true;
OThreadScan.setDiscoverFilters(filters);
```

## Troubleshooting

**Startup order:** Start [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) first and wait for
`Role: Leader`. Then flash this sketch.

| Symptom | Likely cause |
| --- | --- |
| No `found:` lines | Leader not running or out of range — start Leader on another board. |
| `discovery failed to start` | Interface not up or scan already in progress. |
| `discovery complete: failed` | Timeout or internal error — increase `setScanTimeout()`. |
| Live count differs from final count | `onResult()` fires for every Discovery Response (including duplicate name+PAN); stored list merges by name + PAN ID (strongest RSSI kept). |

## See also

* [Thread Network Discovery — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadScan/README.md) — all three discovery patterns.
* [ThreadScan_Discover](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Discover) — blocking `discoverNetworks()`.
* [ThreadScan_Async](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Async) — `scanComplete()` polling without per-result callbacks.
* [OpenThread Core API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_core.html) — `OThread` and stack management.

## License

Apache License 2.0.
