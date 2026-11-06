# LeaderNode - Thread Network Former (Native API)

Leader side of the [Simple Thread Network demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/README.md). This sketch:

* starts OpenThread with `begin(false)` (no NVS auto-load),
* builds a fresh `DataSet` with network name, channel, PAN ID, extended PAN ID,
  and network key,
* commits the dataset and becomes the **Leader** of a new partition,
* prints role, RLOC, network parameters, unicast/multicast addresses, and
  address-cache management every 5 seconds in `loop()`.

It is the counterpart of [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode). Flash
this sketch **before** any Router/Child boards.

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

## Prerequisites

Flash this sketch **before** [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode).

Share the **network key** (and ideally the same channel/PAN) with Router nodes.
The Router sketch only commits the network key locally; other parameters are
learned from this Leader after attach.

## What the sketch does

```cpp
// 1) Start stack without NVS dataset.
threadLeaderNode.begin(false);

// 2) Build and commit a full operational dataset.
dataset.initNew();
dataset.setNetworkName("ESP_OpenThread");
dataset.setChannel(15);
dataset.setPanId(0x1234);
dataset.setExtendedPanId(extPanId);
dataset.setNetworkKey(networkKey);
threadLeaderNode.commitDataSet(dataset);
threadLeaderNode.networkInterfaceUp();
threadLeaderNode.start();

// 3) loop(): print network info every 5 s; clear address cache on role change
```

## Expected serial output

```text
==============================================
OpenThread Network Information:
Role: Leader
RLOC16: 0x0000
Network Name: ESP_OpenThread
Channel: 15
PAN ID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
Mesh Local EID: fdde:ad00:beef:0:....
Leader RLOC: fdde:ad00:beef:0:....
Node RLOC: fdde:ad00:beef:0:....

--- Unicast Addresses (Using Count + Index API) ---
  [0]: fdde:ad00:beef:0:....

--- Multicast Addresses (Using std::vector API) ---
  [0]: ff02::1
```

Before attach:

```text
Thread Node Status: Detached - Waiting for thread network start...
```

## Customization

Edit the dataset in `setup()` before `commitDataSet()`:

| Parameter        | Default in sketch                              |
| ---------------- | ---------------------------------------------- |
| Network name     | `ESP_OpenThread`                               |
| Channel          | `15` (must be 11..26)                          |
| PAN ID           | `0x1234`                                       |
| Extended PAN ID  | `DE AD 00 BE EF 00 CA FE`                      |
| Network key      | `00 11 22 ... EE FF` (16 bytes)                |

All devices in the same network must use the **same network key**. The channel
and PAN should match for predictable joining behavior.

## Troubleshooting

**Startup order:** Flash this **Leader** sketch first. Wait for `Role: Leader`,
then flash [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode) with the matching network key.

| Symptom | Likely cause |
| --- | --- |
| Stuck as Detached | RF issue or invalid dataset — check channel/PAN/key values. |
| Router cannot join | Network key mismatch or Leader not running — verify key bytes match Router sketch. |
| Another device becomes Leader | Conflicting network on same channel/PAN — power off other Thread devices. |
| Changed constants ignored | Both sketches use `begin(false)` but erase flash if stale NVS interferes. |

## See also

* [Simple Thread Network — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/README.md) — two-board bring-up and shared dataset table.
* [Simple Thread Network — RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode) — companion Router/Child joiner sketch.
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — join with PSKd instead of sharing the network key.
* [CLI Leader Node](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/LeaderNode) — CLI equivalent.

## License

Apache License 2.0.
