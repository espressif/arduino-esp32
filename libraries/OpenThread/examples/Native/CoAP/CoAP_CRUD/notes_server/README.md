# notes_server - Thread Leader + Commissioner + CoAP REST Store

Server side of the [CoAP CRUD demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/README.md). This sketch:

* forms a fresh Thread network `ESP_OT_CoAP_CRUD` with a hard-coded
  `DataSet` (`initNew()` on each boot),
* petitions the **Commissioner** role and opens a joiner window for PSKd
  `J01NME`,
* starts a plain **CoAP server** on port **5683** via `OThreadCoAPServer`,
* exposes an in-memory REST collection at path **`notes`** through
  `OThreadCoAPResourceStore` (capacity 8 items),
* handles all REST endpoints in **callback mode** — no CoAP polling in
  `loop()`.

It is the counterpart of [CoAP CRUD — notes_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client).
One server can serve one or more client boards on the same network.

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
| `CONFIG_OPENTHREAD_COMMISSIONER=y`   | Enable Commissioner APIs for Joiner admission.   |

## Prerequisites

Flash this sketch **before** any [CoAP CRUD — notes_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client) boards.

The server calls `ds.initNew()` on every boot, which creates a new network
identity each time the board is reset (unless you change the sketch to resume
from NVS). After any server reset, **reset all clients** so they re-join the
new commissioner.

Wait for `Commissioner ready (PSKd "J01NME")` before flashing or resetting
clients.

## What the sketch does

```cpp
// 1) Form the Thread network and become Leader.
DataSet ds;
ds.initNew();
ds.setNetworkName("ESP_OT_CoAP_CRUD");
ds.setChannel(CHANNEL);
ds.setPanId(PAN_ID);
ds.setNetworkKey(NETKEY);
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
// wait until role >= OT_ROLE_CHILD

// 2) Petition Commissioner and open the joiner window.
OThread.startCommissioner();
OThread.addJoiner("J01NME", JOINER_WINDOW_SEC);

// 3) Start CoAP and attach the REST resource store.
OThreadCoAPServer.begin();
Notes.onChange(onNotesChanged);
Notes.attach(OThreadCoAPServer, "notes", 8);
Serial.printf("Leader: %s\n", OThread.getLeaderRloc().toString().c_str());
```

## REST API

The client uses the printed Leader RLOC as its CoAP destination.

| Method | Path | Action | Response |
| --- | --- | --- | --- |
| GET | `notes` | List all notes (JSON array) | `205 Content` |
| GET | `notes/<id>` | Read one item | `205` or `404` |
| POST | `notes` | Create (JSON body `{"text":"..."}`) | `201 Created` |
| PUT | `notes/<id>` | Update | `204 Changed` or `404` |
| DELETE | `notes/<id>` | Delete | `202 Deleted` or `404` |

POST bodies must be JSON with a `"text"` field. The store prints
`Store 'notes' changed (N items)` via `Notes.onChange()` when the collection
changes.

## Expected serial output

```text
=== CoAP CRUD — notes server ===
Forming Thread network...
Waiting for attach..
Attached as Leader.
Starting Commissioner...
Commissioner ready (PSKd "J01NME")
Starting CoAP server...
Ready. REST endpoints:
  GET    notes       — list
  GET    notes/<id>  — read
  POST   notes       — create
  PUT    notes/<id>  — update
  DELETE notes/<id>  — delete
Leader: fdde:ad00:beef:0:0:ff:fe00:0
Store 'notes' changed (1 items)
```

If network formation fails temporarily:

```text
=== CoAP CRUD — notes server ===
Forming Thread network...
Waiting for attach.........
Attach timeout.
Network failed, retrying in 2s...
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant           | Purpose                                                      |
| ------------------ | ------------------------------------------------------------ |
| `PSKD`             | Joiner secret accepted by the Commissioner.                  |
| `JOINER_WINDOW_SEC`| How long `addJoiner()` stays valid (default 600 s).          |
| `CHANNEL`          | 802.15.4 channel (default 15).                               |
| `PAN_ID`           | 16-bit PAN ID (default `0xC0DE`).                            |
| `NETKEY`           | 128-bit network key for the formed network.                  |
| Store capacity     | Third argument to `Notes.attach()` (default 8 items).        |

CoAP listens on the standard port **5683** (`OThreadCoAPServer.begin()`).
This is correct for application CoAP; do not bind unrelated UDP traffic to
5683/5684 on the same device.

## Troubleshooting

**Startup order:** Flash this **notes_server (Leader + Commissioner)** sketch
first. Wait for `Commissioner ready`, then flash
[CoAP CRUD — notes_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client). **Reset clients** after any server reset.

| Symptom | Likely cause |
| --- | --- |
| `ResourceStore attach failed.` | CoAP server not started or path conflict. |
| Client join fails | Commissioner window closed — reset server or extend `JOINER_WINDOW_SEC`; start server before client. |
| POST returns error | Store full — increase capacity in `Notes.attach()` or delete notes. |
| Client worked, then stops after server reset | Server `initNew()` creates a new network each boot — **reset client** to re-join. |
| Client booted before server ready | Reset client after server prints `Commissioner ready`. |

## See also

* [CoAP CRUD — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/README.md) — REST API table and wire format.
* [CoAP CRUD — notes_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client) — interactive Serial CRUD client.
* [Thread Commissioning — CommissionerNode](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode) —
  Commissioner flow without CoAP traffic.
* [OpenThread CoAP API](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html)

## License

Apache License 2.0.
