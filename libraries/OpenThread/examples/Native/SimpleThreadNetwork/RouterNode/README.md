# RouterNode - Thread Router/Child Joiner (Native API)

Client side of the [Simple Thread Network demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/README.md). This sketch:

* starts OpenThread with `begin(false)` (no NVS auto-load),
* commits a minimal `DataSet` containing **only the network key** (matching the
  Leader),
* calls `networkInterfaceUp()` + `start()` to join the existing network,
* learns channel, PAN ID, extended PAN ID, and network name from the Leader,
* prints the active dataset, role, RLOC, and addresses every 5 seconds.

It is the counterpart of [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode).

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

[LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) must already be running as **Leader** on the same
RF range.

The **network key** in this sketch must match the Leader's key **exactly** (16
bytes). Only the key is required locally — other operational parameters are
learned after attach.

Flash the Leader first and wait for `Role: Leader`, then flash this sketch.
Reset this board if it booted before the Leader was ready.

## What the sketch does

```cpp
// 1) Start stack without NVS dataset.
threadChildNode.begin(false);

// 2) Commit ONLY the network key (must match Leader).
dataset.clear();
dataset.setNetworkKey(networkKey);
threadChildNode.commitDataSet(dataset);
threadChildNode.networkInterfaceUp();
threadChildNode.start();

// 3) loop(): print active dataset + runtime addresses every 5 s
```

## Expected serial output

```text
==============================================
OpenThread Network Information (Active Dataset):
Role: Router
RLOC16: 0xfc00
Network Name: ESP_OpenThread
Channel: 15
PAN ID: 0x1234
Extended PAN ID: dead00beef00cafe
Network Key: 00112233445566778899aabbccddeeff
Mesh Local EID: fdde:ad00:beef:0:....
Node RLOC: fdde:ad00:beef:0:....
Leader RLOC: fdde:ad00:beef:0:0:ff:fe00:0
```

Before attach:

```text
Thread Node Status: Detached - Waiting for thread network start...
```

## Customization

The only required local configuration is the **network key** in `setup()`:

```cpp
uint8_t networkKey[OT_NETWORK_KEY_SIZE] = {
  0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};
dataset.setNetworkKey(networkKey);
```

This byte array must match [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) exactly.

## Troubleshooting

**Startup order:** Start [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) first and wait for
`Role: Leader`. Then flash this Router sketch. Reset if it booted before the
Leader was up.

| Symptom | Likely cause |
| --- | --- |
| Stuck as Detached | Leader not running or network key mismatch — verify key bytes match Leader. |
| Wrong network name/channel after attach | Joined a different partition — erase flash on both boards and retry. |
| Attaches as Child instead of Router | Normal on small networks — role may upgrade over time. |
| Changed key has no effect | Stale NVS — erase flash; both sketches use `begin(false)` but prior datasets may linger. |

## See also

* [Simple Thread Network — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/README.md) — two-board bring-up and shared dataset table.
* [Simple Thread Network — LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode) — companion Leader network former.
* [Thread Commissioning — JoinerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) — join without pre-sharing the network key.
* [CLI Router Node](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/SimpleThreadNetwork/RouterNode) — CLI equivalent.

## License

Apache License 2.0.
