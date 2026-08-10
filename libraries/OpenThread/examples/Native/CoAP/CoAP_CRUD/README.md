# CoAP CRUD — REST Resource Store over Thread (Native API)

This folder contains a two-board example that exposes an in-memory REST
collection as CoAP resources using the Arduino OpenThread Native API
(`OThread` + `OThreadCoAP`). The pair uses Thread commissioning for network
admission and confirmable CoAP on the standard CoAP port **5683**.

| Sketch | Role |
| --- | --- |
| [notes_server (Leader + REST server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server) | Server: Thread **Leader + Commissioner**, plain CoAP server, and `OThreadCoAPResourceStore` on path `notes`. |
| [notes_client (Joiner + Serial CLI)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client) | Client: Thread **Joiner** and interactive Serial CLI driving CRUD operations. |

The server forms network `ESP_OT_CoAP_CRUD`, opens a Commissioner join window
for PSKd `J01NME`, and serves REST handlers in **callback mode** (no CoAP
polling in `loop()`). The client joins that network, resolves the Leader RLOC
as its CoAP destination, and sends confirmable GET/POST/PUT/DELETE requests.

## REST API

Base path: **`notes`**

| Method | Path | Action | Response |
| --- | --- | --- | --- |
| GET | `notes` | List all notes (JSON array) | `205 Content` |
| GET | `notes/2` | Read note id `2` | `205` or `404` |
| POST | `notes` | Create note (JSON body) | `201 Created` + `Location-Path: notes/N` |
| PUT | `notes/2` | Update note `2` | `204 Changed` or `404` |
| DELETE | `notes/2` | Delete note `2` | `202 Deleted` or `404` |

POST and PUT bodies are JSON (`{"text":"..."}`). The client sets
`CoapClient.setContentFormat(OT_COAP_FORMAT_JSON)`; the resource store responds
with JSON and sets the Content-Format option on each reply. GET/DELETE have no
request payload, so Content-Format applies only to responses on those methods.

## How to Run

1. Flash [notes_server (Leader + REST server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server) on one
   ESP32-H2 / ESP32-C6 / ESP32-C5 board and open Serial Monitor at `115200`.
2. Wait for `Commissioner ready (PSKd "J01NME")` and the printed Leader address.
3. Flash [notes_client (Joiner + Serial CLI)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client) on a
   second board and wait for `Attached. Notes server: ...`.
4. Type Serial commands on the client (`list`, `add buy milk`, `read 1`, etc.).

Each sketch has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | Both sketches |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | Both sketches |
| `CONFIG_OPENTHREAD_COMMISSIONER=y` | `notes_server` |
| `CONFIG_OPENTHREAD_JOINER=y` | `notes_client` |

## Troubleshooting

**Startup order:** Flash [CoAP CRUD — notes_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_server) first and wait for
`Commissioner ready`. Then flash [CoAP CRUD — notes_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client). The client
joins only in `setup()` — reset the client if it booted before the server was
ready. The server calls `initNew()` on each boot (new network identity) —
reset the client after any server reboot.

| Symptom | Likely cause |
| --- | --- |
| Client prints `Join failed, retry in 3s...` | Server not ready — start server first, then reset client. |
| Client shows `Attached` but Serial commands fail | Server was reset after join — reset client to re-join the new network. |
| `-> error timeout` on CRUD commands | Server unreachable, stale Leader RLOC, or client on old network after server reset. |
| Client join works without server running | Prior NVS dataset on client — erase NVS or reset after server forms the intended network. |

See per-sketch READMEs for full symptom tables.

## See Also

* [Native CoAP examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md) — all Native CoAP demos and port conventions.
* [CLI CoAP examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) — CLI-based CoAP demos for comparison.
* [Thread Commissioning examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning) — Joiner / Commissioner flow without CoAP traffic.

## License

Apache License 2.0.
