# OpenThread Native API Examples (Arduino)

This folder contains sketches that use the **OpenThread Native API** from
Arduino code.

## What is the OpenThread Native API?

The Native API is the typed C++ interface exposed by Arduino wrappers such as:

- `OThread`
- `DataSet`
- `OThreadUDP`
- `OThreadCoAP` (`OThreadCoAPClient`, `OThreadCoAPServer`, …)

Instead of sending textual OpenThread CLI commands, sketches call methods
directly, for example:

- `OThread.begin(false)`
- `OThread.commitDataSet(ds)`
- `OThread.networkInterfaceUp()`
- `OThread.start()`
- `OThread.startJoiner(...)` then `OThread.start()` on success (Joiner side)
- `OThread.start()` then `OThread.startCommissioner()` when attached (Commissioner side)
- `otUdp.begin(...)`
- `otUdp.beginPacket(...)`

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

Commissioning call order (see [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) and [Native UDP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP)):

| Role | Order after `begin(false)` |
| --- | --- |
| Joiner | `networkInterfaceUp()` → `startJoiner(PSKD)` → `start()` on success |
| Commissioner | `commitDataSet()` (or NVS resume) → `networkInterfaceUp()` → `start()` → wait for attach → `startCommissioner()` → `addJoiner()` |

Do **not** call `start()` before `startJoiner()`; OpenThread returns
`OT_ERROR_INVALID_STATE` if Thread is already enabled during Joiner
commissioning.

### Network identity: `initNew()` vs NVS resume

Multi-board demos behave differently depending on how each sketch obtains its
Thread identity:

| Pattern | Typical sketches | After server reboot |
| --- | --- | --- |
| **`initNew()` every boot** | Most CoAP servers (SimpleGet, Light Switch, CRUD, Sensor server, Secure/Greenhouse servers) | Forms a **new** partition (new Extended PAN ID). Clients must **reset or re-flash** to re-join. |
| **NVS resume** (`begin(true)` or `commitDataSet` without `initNew`) | UDP Light Switch `light`, some Commissioner flows | Same network identity when NVS is intact. Clients usually re-attach without erase. |
| **Network key only (client)** | CoAP SimpleGet `client`, CoAP Sensor `sensor_client`, Native/CLI `RouterNode` | Joins whichever Leader matches `NETKEY`; fails if server rebooted with a fresh `initNew()` partition. |
| **Joiner + PSKd (no local dataset)** | CoAP Light Switch `switch`, UDP Light Switch `switch`, [Thread Commissioning — JoinerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) | Needs an open Commissioner window (`addJoiner`); not the same as NETKEY-only join. |

Before changing dataset constants in source, **erase flash** (or factory-reset
the OpenThread dataset) on all boards. If a client shows `Started as Leader` or
attach timeout after a server reboot, the server likely started a fresh
`initNew()` network — reset the client after the server is Leader again.

### Commissioner join-window patterns

| Pattern | When `addJoiner()` runs | Examples |
| --- | --- | --- |
| **Automatic** | Right after `startCommissioner()` succeeds | CoAP Light Switch `light`, UDP Light Switch `light`, CoAP CRUD `notes_server`, CoAP Secure/Greenhouse servers |
| **Button-gated** | On user button press after Commissioner is active | [Thread Commissioning — CommissionerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) |

If you flash a joiner sketch while no Commissioner window is open, attach fails
until the server calls `addJoiner()` (or you press the Commissioner button).

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
otUdp.begin(localPort);
otUdp.beginPacket(peerAddress, peerPort);
otUdp.write(payload, length);
otUdp.endPacket();
```

### `OThreadCoAP`

`OThreadCoAP*` classes wrap the OpenThread Application CoAP API (`otCoap*`) in an
Arduino-style interface. Plain CoAP uses UDP port **5683**; CoAPS (DTLS) uses
port **5684** when enabled at build time.

Typical server usage (global singleton — do not declare a local server variable):

```cpp
static void onHello(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  resp.setCode(OT_COAP_RESP_OK);
  resp.setPayload("Hello from CoAP!");
  resp.send();
}

OThreadCoAPServer.on("hello", OT_COAP_METHOD_GET, onHello);
OThreadCoAPServer.begin();
```

Typical client usage:

```cpp
OThreadCoAPClient client;
client.setConfirmable(true);
int code = client.GET(serverIp, "hello");
```

See the [Native CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP) for full two-board CoAP demos. CLI-based
CoAP examples remain under [CLI CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) for reference.

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
| [Native StackShutdown](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown) | [StackShutdown](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown) | Graceful teardown then restart: `OThreadCoAPServer.stop()` → `OThreadUDP.stop()` → `OThread.end()` → `setup()`. |
| [Simple Thread Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork) | [LeaderNode (network former)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode), [RouterNode (joiner)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/RouterNode) | Basic Native network formation and joining using `OThread` and `DataSet`. |
| [Thread Commissioning](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) | [CommissionerNode (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode), [JoinerNode (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) | Thread commissioning: a Commissioner opens a joiner window and a Joiner obtains the dataset using only a PSKd. |
| [Native UDP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP) | [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch), [UDP Sensor Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork) | Native UDP application traffic (ports 5050/5051). See [Native UDP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/README.md). |
| [Native CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP) | [CoAP SimpleGet](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet), [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch), [CoAP Sensor](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor), [CoAP CRUD](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD), [CoAP Secure](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure), [CoAP Greenhouse](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse) | Native CoAP / CoAPS on ports 5683/5684. See [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md). |

## Choosing an example

- Use [Native StackShutdown](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown) if you need a working Native teardown
  sequence with UDP and CoAP before `OThread.end()`, plus a `setup()` restart without chip reset.
- Start with [Simple Thread Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork) if you only need to learn how to form and
  join a Thread network with a preconfigured dataset.
- Use [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) if devices should join securely using a PSKd
  instead of carrying the network key in source code.
- Use [UDP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch) if you want a compact command/ACK example over UDP.
- Use [UDP Sensor Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork) if you want a more robust telemetry pattern with
  application-level acknowledgments and node liveness tracking.
- Use [CoAP SimpleGet](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet) for the smallest introduction to `OThreadCoAPClient` and
  `OThreadCoAPServer`.
- Use [CoAP Light Switch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) for multicast command/response patterns over CoAP port 5683.
- Use [CoAP Sensor](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor) for a read-only resource with changing values and NON polling.
- Use [CoAP CRUD](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD) for REST collections with `OThreadCoAPResourceStore`.
- Use [CoAP Secure](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure) or [CoAP Greenhouse](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse) when CoAPS (DTLS) is required.

## Practical guidance

- Check return values from Native APIs, especially `otError` results from
  Joiner / Commissioner calls.
- Bind UDP sockets only after the device has attached to a Thread network.
- Avoid OpenThread-reserved UDP ports for application traffic. For UDP examples,
  `5050` and `5051` are used instead of CoAP / TMF ports. For CoAP examples,
  use application ports **5683** (plain) and **5684** (CoAPS) — not **61631**
  (Thread TMF CoAP).
- If a sketch resumes a dataset from NVS, erase NVS or factory-reset the
  OpenThread dataset before expecting changed dataset constants to take effect.
- For commissioning examples, make sure the Commissioner joiner window is open
  before starting or retrying the Joiner.
- To stop Thread without rebooting, follow the shutdown order in the [OpenThread library README](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/README.md):
  [Native StackShutdown](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown) (Native UDP + CoAP teardown and `setup()` restart) or [CLI StackShutdown](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/StackShutdown) (CLI-only).

## Troubleshooting

Multi-board demos (UDP, CoAP, SimpleThreadNetwork, ThreadCommissioning) share the same bring-up rule: **start the server / Leader / Commissioner / collector sketch first**, wait until Serial reports attached (and Commissioner ready when commissioning is used), then flash or **reset** client / Joiner / router boards that booted too early.

| Symptom | Likely cause |
| --- | --- |
| Joiner or client cannot attach | Server/Leader not running yet, join window closed, or client started before server was ready — reset client after server is up. |
| Attached but application traffic fails | Server not listening, wrong port, stale Leader RLOC, or dataset mismatch between boards. |
| Works once, fails after reboot | Demo may use `initNew()` (new network each boot) vs NVS resume — erase NVS or reset clients to re-join; see the specific example README. |
| `OT_ERROR_INVALID_STATE` on Joiner | Called `start()` before `startJoiner()` — use Joiner call order from the table above. |
| Build errors for Joiner/Commissioner | Missing `CONFIG_OPENTHREAD_JOINER=y` or `CONFIG_OPENTHREAD_COMMISSIONER=y` in sdkconfig for that sketch. |

## License

Apache License 2.0.
