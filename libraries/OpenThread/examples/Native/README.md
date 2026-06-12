# OpenThread Native API Examples (Arduino)

This folder contains sketches that use the **OpenThread Native API** from
Arduino code.

## What is the OpenThread Native API?

The Native API is the typed C++ interface exposed by Arduino wrappers such as:

- `OThread`
- `DataSet`
- `OThreadUDP`

Instead of sending textual OpenThread CLI commands, sketches call methods
directly, for example:

- `OThread.begin(false)`
- `OThread.commitDataSet(ds)`
- `OThread.networkInterfaceUp()`
- `OThread.start()`
- `OThread.startJoiner(...)` then `OThread.start()` on success (Joiner side)
- `OThread.start()` then `OThread.startCommissioner()` when attached (Commissioner side)
- `Udp.begin(...)`
- `Udp.beginPacket(...)`

This style keeps application logic in structured C++ code, gives clearer return
values (`otError`, booleans, typed getters), and avoids parsing CLI text output.

## Core Native classes

### `OThread`

`OThread` is the main entry point for managing the OpenThread stack. It provides
helpers for stack startup, Thread interface control, role checks, address
queries, dataset handling, and, when enabled, Joiner / Commissioner operations.

Typical usage (form or resume a network with a known dataset):

```cpp
OThread.begin(false);
OThread.networkInterfaceUp();
OThread.start();

Serial.println(OThread.otGetStringDeviceRole());
Serial.println(OThread.getMeshLocalEid());
```

Commissioning call order (see `ThreadCommissioning/` and UDP examples):

| Role | Order after `begin(false)` |
| --- | --- |
| Joiner | `networkInterfaceUp()` → `startJoiner(PSKD)` → `start()` on success |
| Commissioner | `commitDataSet()` (or NVS resume) → `networkInterfaceUp()` → `start()` → wait for attach → `startCommissioner()` → `addJoiner()` |

Do **not** call `start()` before `startJoiner()`; OpenThread returns
`OT_ERROR_INVALID_STATE` if Thread is already enabled during Joiner
commissioning.

### `DataSet`

`DataSet` wraps the Thread Operational Dataset. It is used when a sketch needs
to form or pre-configure a network with known parameters:

```cpp
DataSet ds;
ds.initNew();
ds.setNetworkName("ESP_OpenThread");
ds.setChannel(15);
ds.setPanId(0x1234);
ds.setNetworkKey(networkKey);
OThread.commitDataSet(ds);
```

Use this approach for examples that form a network, such as Leader or
Commissioner sketches.

### `OThreadUDP`

`OThreadUDP` is an Arduino `UDP`-compatible class backed directly by the
OpenThread `otUdpSocket` API. It is used by the Native UDP examples for IPv6 UDP
traffic over the Thread mesh without using lwIP.

Typical usage:

```cpp
Udp.begin(localPort);
Udp.beginPacket(peerAddress, peerPort);
Udp.write(payload, length);
Udp.endPacket();
```

## Native vs CLI: quick comparison

| Topic | Native approach | CLI approach |
| --- | --- | --- |
| Interface style | Typed methods/functions | String commands |
| Error handling | Return values (`otError`, booleans) | Parse CLI responses (`Done`, `Error ...`) |
| Best for | Production-oriented application logic, compile-time checks, maintainability | Feature parity with the OT shell, quick prototyping, command-driven flows |
| Debug visibility | Cleaner runtime logic, less text parsing | Very explicit command/response logs |
| Dependency on wrappers | Depends on wrapper coverage for each feature | Low: can use features directly if CLI supports them |

## When should I use Native API?

Use Native API when you want to:

- build production-oriented logic with stronger typing,
- reduce string parsing and command formatting code,
- simplify long-term maintenance and refactoring,
- keep network state checks and application behavior in structured C++ code,
- use `OThreadUDP` for application UDP traffic over Thread.

## When should I use CLI?

Use CLI when you want to:

- mirror known OpenThread shell commands directly,
- validate behavior quickly with command-like flows,
- use a capability exposed in CLI before a Native wrapper exists,
- keep behavior close to CLI/manual operational procedures.

## Can I mix Native and CLI in the same sketch?

**Yes.** The Arduino OpenThread Library supports mixed usage.

Common mixed pattern:

- use **Native** APIs for startup, state checks, and application logic,
- use **CLI** commands for diagnostics or for OpenThread features that do not
  yet have a Native wrapper,
- document clearly which layer owns each operation.

Prefer one dominant style per sketch for readability.

## Required target support

These examples require an ESP32 target with IEEE 802.15.4 / Thread support, such
as ESP32-H2, ESP32-C6, or ESP32-C5.

Common requirements:

- `CONFIG_OPENTHREAD_ENABLED=y`
- `CONFIG_SOC_IEEE802154_SUPPORTED=y`

Some examples also require:

- `CONFIG_OPENTHREAD_JOINER=y` for Joiner sketches
- `CONFIG_OPENTHREAD_COMMISSIONER=y` for Commissioner sketches

## Related examples in this folder

| Folder | Sketches | What it demonstrates |
| --- | --- | --- |
| `SimpleThreadNetwork/` | `LeaderNode`, `RouterNode` | Basic Native network formation and joining using `OThread` and `DataSet`. |
| `ThreadCommissioning/` | `CommissionerNode`, `JoinerNode` | Thread commissioning: a Commissioner opens a joiner window and a Joiner obtains the dataset using only a PSKd. |
| `UDP_Light_Switch/` | `light`, `switch` | Two-board UDP light control demo using Joiner / Commissioner and `OThreadUDP` on application port `5051`. |
| `UDP_SensorNetwork/` | `sensor_collector`, `sensor_node` | Many-to-one UDP telemetry demo with sequence ACKs, collector-side node tracking, and optional Sleepy End Device behavior. |

## Choosing an example

- Start with `SimpleThreadNetwork/` if you only need to learn how to form and
  join a Thread network with a preconfigured dataset.
- Use `ThreadCommissioning/` if devices should join securely using a PSKd
  instead of carrying the network key in source code.
- Use `UDP_Light_Switch/` if you want a compact command/ACK example over UDP.
- Use `UDP_SensorNetwork/` if you want a more robust telemetry pattern with
  application-level acknowledgements and node liveness tracking.

## Practical guidance

- Check return values from Native APIs, especially `otError` results from
  Joiner / Commissioner calls.
- Bind UDP sockets only after the device has attached to a Thread network.
- Avoid OpenThread-reserved UDP ports for application traffic. For these
  examples, `5050` and `5051` are used instead of CoAP / TMF ports.
- If a sketch resumes a dataset from NVS, erase NVS or factory-reset the
  OpenThread dataset before expecting changed dataset constants to take effect.
- For commissioning examples, make sure the Commissioner joiner window is open
  before starting or retrying the Joiner.

## License

Apache License 2.0.
