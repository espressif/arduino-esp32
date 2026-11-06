# notes_client - Thread Joiner + CoAP REST Client

Client side of the [CoAP CRUD demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/README.md). This sketch:

* runs the **Thread Joiner** state machine in `setup()` using PSKd `J01NME`
  and a channel hint, then calls `start()` after successful commissioning,
* resolves the notes server as the Thread **Leader RLOC** returned by
  `OThread.getLeaderRloc()`,
* exposes an interactive **Serial command menu** for REST operations on path
  `notes` (`list`, `read`, `add`, `update`, `del`),
* sends **confirmable** CoAP requests (`setConfirmable(true)`) with
  **JSON Content-Format** (`setContentFormat(OT_COAP_FORMAT_JSON)`) on POST/PUT
  bodies, and prints the response code, status string, and JSON payload.

It is the counterpart of [CoAP CRUD — notes_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server).
Flash on one board while the server sketch runs on another in RF range.

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
| `CONFIG_OPENTHREAD_JOINER=y`         | Enable `otJoiner*` APIs used by the sketch.      |

## Prerequisites

A [CoAP CRUD — notes_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server) instance must already be running on the
same RF range.

For the client's **first join** (or after erasing NVS / factory reset), the
server must have reached `Commissioner ready (PSKd "J01NME")` and still be
within its joiner window (`JOINER_WINDOW_SEC` on the server — default 600 s).

The client joins **only in `setup()`** (with retries and `stop()` between
attempts). If the server was not ready when the client booted, press **reset**
on the client after the server Commissioner is active.

After a **server reset**, the server calls `initNew()` and forms a new network
— reset the client to re-join.

## What the sketch does

```cpp
// 1) Bring OT stack up and configure the CoAP client.
OThread.begin(false);
CoapClient.setTimeout(4000);
CoapClient.setConfirmable(true);

// 2) Join the notes_server network (retries until attached).
OThread.setChannel(CHANNEL_HINT);
OThread.networkInterfaceUp();
OThread.startJoiner("J01NME", JOIN_TIMEOUT_MS);
OThread.start();
serverIp = OThread.getLeaderRloc();

// 3) In loop(): parse Serial commands and issue CoAP CRUD calls.
printResult(CoapClient.GET(serverIp, "notes"));           // list
printResult(CoapClient.POST(serverIp, "notes", body));     // add
printResult(CoapClient.PUT(serverIp, "notes/1", body));     // update
printResult(CoapClient.DELETE(serverIp, "notes/1"));       // del
```

## Expected serial output

```text
=== CoAP CRUD — notes client ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Waiting for attach..
Attached as Child. Notes server: fdde:ad00:beef:0:0:ff:fe00:0
Commands: list | read <id> | add <text> | update <id> <text> | del <id>
Ready. Enter a command.
list
-> 205 2.05 Content
   []
add buy milk
-> 201 2.01 Created
   {"id":1,"text":"buy milk"}
```

If the server is not ready at boot:

```text
=== CoAP CRUD — notes client ===
Joining Thread network (Joiner)...
Commissioning with PSKd "J01NME"...
Joiner failed: 7
Join failed, retry in 3s...
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Pre-Shared Key for Device. Must match the server Commissioner. |
| `CHANNEL_HINT`     | 802.15.4 channel hint (skip scan). Must match the server network. |
| `JOIN_TIMEOUT_MS`  | Max wait inside `startJoiner()` for the commissioner.        |

Serial commands (type in Serial Monitor at `115200`):

| Command | CoAP operation |
| --- | --- |
| `list` | GET `notes` |
| `read <id>` | GET `notes/<id>` |
| `add <text>` | POST `notes` body: `{"text":"..."}` |
| `update <id> <text>` | PUT `notes/<id>` |
| `del <id>` | DELETE `notes/<id>` |
| `help` | Print command summary |

**Note:** `add` / `update` build JSON with simple string concatenation. Text
containing `"` or `\` is not escaped and will produce invalid JSON — use plain
text only, or build/escape JSON in your own application code.

CoAP client timeout is set in `setup()` via `CoapClient.setTimeout(4000)` (ms).

## Troubleshooting

**Startup order:** Start [CoAP CRUD — notes_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server) first and wait for
`Commissioner ready`. This client joins **only in `setup()`** — reset this
board if it booted before the server was ready.

| Symptom | Likely cause |
| --- | --- |
| `Joiner failed: <code>` then `Join failed, retry in 3s...` | First attach attempt failed (see numeric code); sketch retries every 3 s. |
| `Join failed, retry in 3s...` only | Server Commissioner not ready — start server first, then reset client. |
| `-> error timeout` | Server unreachable, wrong Leader RLOC, or server reset (client still on old network). |
| `-> error not attached` | Stack detached after attach — reset client. |
| `404 Not Found` on read | Invalid note id. |
| Commands fail after server reset | Server formed a new network (`initNew()` each boot) — **reset client** to re-join. |
| Client booted before server ready | Join only runs in setup — press **reset** after server Commissioner is active. |

## See also

* [CoAP CRUD — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/README.md) — REST API table and wire format.
* [CoAP CRUD — notes_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server) — companion REST notes server.
* [Thread Commissioning — JoinerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode) —
  same Joiner flow without CoAP traffic.
* [OpenThread CoAP API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html)

## License

Apache License 2.0.
