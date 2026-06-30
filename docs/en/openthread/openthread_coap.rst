################
OThreadCoAP APIs
################

About
-----

The ``OThreadCoAP`` classes wrap the OpenThread **Application CoAP** C API
(``otCoap*`` / ``otCoapSecure*``) in an Arduino-style surface similar to
``HTTPClient`` and ``WebServer``. Sketches use typed methods instead of CLI
strings such as ``coap get`` / ``coap put``.

Multicast group commands (``joinMulticastGroup``, NON send, RFC 7252 §8.1/§8.2):
:doc:`Multicast guide <Multicasting>`.

**Key Features:**

* **Plain CoAP client** (``OThreadCoAPClient``) — blocking GET/PUT/POST/DELETE
  to IPv6 unicast or multicast on port **5683**.
* **Plain CoAP server** (``OThreadCoAPServer``) — ``on(path, methodMask, handler)``
  resource registration with callback dispatch (no ``loop()`` polling by default).
* **CoAPS client / server** (``OThreadCoAPSecureClient`` / ``OThreadCoAPSecureServer``)
  — DTLS on port **5684** when ``OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE`` is set.
* **REST helper** (``OThreadCoAPResourceStore``) — in-memory CRUD collections
  under a base path (for example ``notes``, ``notes/1``).
* **Confirmable vs non-confirmable** — ``setConfirmable(true/false)`` on clients
  maps to CLI ``con`` / ``non-con``; server responses mirror incoming CON/NON.
  Full guide: :ref:`coap_con_non`; multicast vs CON: :ref:`coap_multicast_con`.

**Build requirements:**

* Plain CoAP: enabled by default on ESP32 OpenThread builds
  (``OPENTHREAD_CONFIG_COAP_API_ENABLE=1``).
* CoAPS: requires ``OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1`` and mbedtls
  PSK or ECDHE cipher suite flags.

**Reserved ports:**

* **5683** — application CoAP (``OT_COAP_DEFAULT_PORT``).
* **5684** — CoAPS (``OT_COAP_SECURE_DEFAULT_PORT``).
* **61631** — Thread TMF CoAP (internal; do not bind application servers here).

Include header:

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadCoAP.h>

OThreadCoAP (static helpers)
----------------------------

.. code-block:: arduino

    class OThreadCoAP {
    public:
        static bool isAttached();
        static bool secureApiEnabled();
        static const char *responseCodeToString(int code);
        static const char *errorToString(int error);
        static OThreadCoAPServerClass& plainServer();
        static OThreadCoAPSecureServerClass& secureServer();
    };

* ``isAttached()`` — ``true`` when the device role is Child or higher.
* ``secureApiEnabled()`` — ``true`` when CoAPS was compiled in
  (``OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1`` at build time). Use at
  runtime before using ``OThreadCoAPSecureClient`` /
  ``OThreadCoAPSecureServer`` so sketches can fall back gracefully when the
  secure API is disabled.
* ``plainServer()`` / ``secureServer()`` — return the same singleton instances as
  the globals ``OThreadCoAPServer`` and ``OThreadCoAPSecureServer`` (see below).
* ``responseCodeToString()`` — human-readable CoAP response (for example
  ``"2.05 Content"`` for ``OT_COAP_RESP_OK``).
* ``errorToString()`` — maps negative ``OT_COAP_ERROR_*`` values (for example
  ``"not attached"`` vs ``"invalid state"``).

Stopping the stack
------------------

By default, ``setTimeout()`` on ``OThreadCoAPClient`` also tunes confirmable
(CON) retransmission so OpenThread gives up within about the same limit. A
returned ``OT_COAP_ERROR_TIMEOUT`` then means no response within that window.

If you call ``useDefaultCoapRetransmit()``, wire retries revert to RFC 7252
defaults (~93 s exchange) while the sketch still caps at ``setTimeout()``. In
that mode a blocking call can return ``OT_COAP_ERROR_TIMEOUT`` while OpenThread
still has the request in flight. The wrapper keeps the internal response context
alive until the stack finishes, so a normal timeout never leaks.

CoAPS (``OThreadCoAPSecureClient``) cannot tune per-request wire retries;
``setTimeout()`` is the sketch wait cap only and background retransmits may
continue after timeout.

* ``OThread.stop()`` only disables the network; the worker task keeps running, so
  in-flight handlers still fire and free their contexts. Nothing to clean up.
* ``OThread.end()`` tears the worker task down, so those handlers can no longer
  fire. ``end()`` reclaims any orphaned contexts automatically as part of teardown.

.. _coap_con_non:

Confirmable vs non-confirmable (CON / NON)
------------------------------------------

CoAP (RFC 7252) labels every message **confirmable (CON)** or **non-confirmable
(NON)**. The choice affects reliability, blocking behavior, and whether the
stack retransmits when no answer arrives.

What CON and NON mean
*********************

.. list-table::
   :header-rows: 1
   :widths: 10 22 68

   * - Type
     - CLI keyword
     - Behavior
   * - **CON**
     - ``con``
     - The sender expects an **ACK** at the CoAP message layer. If no ACK or
       response arrives within the retransmission window, OpenThread **retries**
       the request (plain CoAP only). Blocking ``GET`` / ``PUT`` / ``POST`` /
       ``DELETE`` wait until a response or ``OT_COAP_ERROR_TIMEOUT``.
   * - **NON**
     - ``non-con`` (omit ``con`` in CLI defaults to NON for some commands)
     - **Fire-and-forget** at the message layer: no ACK for the request itself.
       Lower overhead; suitable for periodic telemetry where a missed sample is
       acceptable. The server may still send a response (also CON or NON).

**Default:** ``OThreadCoAPClient`` and ``OThreadCoAPSecureClient`` default to
**CON** (``setConfirmable(true)``).

Client API
**********

.. code-block:: arduino

    OThreadCoAPClient client;
    client.setConfirmable(true);   // CON — reliable unicast (default)
    int code = client.GET(serverIp, "hello");

    client.setConfirmable(false);  // NON — polling, telemetry
    code = client.GET(serverIp, "sensor/temp");

    // Multicast / fire-and-forget (always NON internally):
    client.sendNonBlocking(group, OT_COAP_REQ_PUT, "Lamp", (const uint8_t *)"1", 1);

* ``setConfirmable(bool)`` applies to the **next** blocking
  ``GET`` / ``PUT`` / ``POST`` / ``DELETE`` calls on that client instance until
  changed again.
* ``sendNonBlocking(...)`` **always** sends NON and never blocks, regardless of
  ``setConfirmable()``. Use it for multicast commands and unicast telemetry where
  no reply is needed.
* **Multicast destinations** (IPv6 first byte ``0xFF``): blocking methods
  **force NON** even when ``setConfirmable(true)`` — RFC 7252 §8.1 forbids
  confirmable multicast. The client may still wait for the **first** response if
  you use a blocking method, but that does not scale when several servers share
  the group; prefer ``sendNonBlocking()`` and unicast when you need one confirmed
  answer. See :doc:`Multicast guide <Multicasting>`.

Timeouts and CON retransmission (plain client)
**********************************************

For **confirmable** plain CoAP requests only:

* ``setTimeout(ms)`` caps how long the sketch **blocks** and, by default, tunes
  wire retransmission so OpenThread gives up within about the same window.
  ``OT_COAP_ERROR_TIMEOUT`` means no response within that limit.
* Values below ``OT_COAP_MIN_ALIGNED_TIMEOUT_MS`` (default **1500** ms) are
  raised automatically.
* ``useDefaultCoapRetransmit()`` (call **before** ``setTimeout()``) keeps RFC
  7252 fixed wire retries (~93 s) while the sketch still blocks only for
  ``setTimeout(ms)``. A timeout return does not mean the request was canceled
  on the wire.

**NON** requests do not use CON retransmission tuning — they are sent once
(plain CoAP).

**CoAPS** (``OThreadCoAPSecureClient``): ``setConfirmable()`` still selects CON
vs NON for each request, but **per-request wire retries are not tunable**;
``setTimeout()`` is only the sketch wait cap and background retransmits may
continue after timeout.

.. _coap_multicast_con:

Multicast and CON
*****************

Multicast CoAP and confirmable (CON) messages **do not combine**. The library
enforces RFC 7252 at send time:

.. list-table::
   :header-rows: 1
   :widths: 32 68

   * - Situation
     - What the library does
   * - ``setConfirmable(true)`` + destination ``0xFF…``
     - Request is sent as **NON** anyway (§8.1). The confirmable flag is ignored
       for multicast destinations.
   * - CON retransmission / ``setTimeout()`` tuning
     - **Does not apply** to multicast sends — they are always NON and are not
       CON-retried on the wire.
   * - Blocking ``GET`` / ``PUT`` / ``POST`` / ``DELETE`` to a multicast group
     - Still transmitted as **NON**, but the call may **block** until the first
       response or timeout. That is a poor fit when several servers share the
       group (ambiguous / duplicate answers). Prefer ``sendNonBlocking()``.
   * - ``sendNonBlocking()`` to multicast
     - Always **NON**, never blocks, no response waited — the intended API for
       group commands (``CoAP_Light_Switch`` switch).
   * - Server receives multicast request
     - Payload is always from a **NON** request in practice. Call
       ``joinMulticastGroup()`` (or ``subscribeMulticast()``) so the node receives
       group traffic. Check ``req.isMulticast()`` and usually **skip**
       ``resp.send()`` (§8.2). ``req.isConfirmable()`` is false for these requests.
   * - Need a **confirmed** command to one lamp
     - Send **unicast CON** to that device's IPv6 address, not to the group.

The CLI ``coap_switch`` example sends CON to a multicast group for demonstration;
the Native ``CoAP_Light_Switch`` switch uses ``sendNonBlocking()`` (NON) instead.
See :doc:`Multicast guide <Multicasting>` for group join/leave and example paths.

Server behavior
***************

Handlers receive ``OThreadCoAPRequest`` and reply with ``OThreadCoAPResponse``:

* ``req.isConfirmable()`` — ``true`` if the incoming request was CON.
* ``req.isMulticast()`` — ``true`` for group-addressed requests; usually **do
  not** call ``resp.send()`` (RFC 7252 §8.2 — avoids a response storm).
* **Response type mirrors the request:** CON requests get an **ACK** response;
  NON requests get a **NON** response (handled inside the library when you call
  ``resp.send()``).

Every registered path accepts both CON and NON; choose the client mode with
``setConfirmable()``. ``serve()``-backed resources answer **unicast** requests
only (multicast ``serve()`` traffic is ignored).

When to use which
*****************

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Pattern
     - Recommended setting
   * - One-shot command to a **known server** (CRUD, actuators, config)
     - **CON** — ``setConfirmable(true)`` + blocking ``PUT`` / ``POST``; check
       return code or payload.
   * - Periodic **telemetry poll** (temperature, status)
     - **NON** — ``setConfirmable(false)``; missed poll is acceptable.
   * - **Multicast** command to many lamps / actuators
     - ``sendNonBlocking()`` (NON); server ``joinMulticastGroup()`` and skip
       reply on ``req.isMulticast()``.
   * - **CoAPS** secured commands
     - **CON** in examples (``CoAP_Secure``, ``CoAP_Greenhouse`` actuators) for
       delivery assurance within ``setTimeout()``.

Examples in this library
************************

.. list-table::
   :header-rows: 1
   :widths: 32 18 50

   * - Example
     - CON / NON
     - Notes
   * - `CoAP_SimpleGet`_ (client)
     - CON
     - ``setConfirmable(true)`` + blocking GET ``hello``
   * - `CoAP_Sensor`_ (client)
     - NON
     - ``setConfirmable(false)`` periodic GET ``sensor/temp``
   * - `CoAP_CRUD`_ (client)
     - CON
     - Confirmable POST/PUT/DELETE to notes collection
   * - `CoAP_Light_Switch`_ (switch)
     - NON
     - ``sendNonBlocking()`` multicast PUT; no response expected
   * - `CoAP_Greenhouse`_ (client)
     - NON + CON
     - NON GET telemetry on plain CoAP; CON PUT actuators on CoAPS
   * - CLI ``coap_switch``
     - CON
     - CLI ``con`` keyword on multicast PUT (demo only; Native switch uses NON)

OThreadCoAPClient
-----------------

Blocking HTTPClient-style client over plain CoAP (UDP 5683).

.. code-block:: arduino

    OThreadCoAPClient client;
    client.setTimeout(5000);
    client.setConfirmable(true);   // CON — CLI "con"
    int code = client.GET(serverIp, "hello");
    if (code >= 0) {
        Serial.println(client.getString());
    }

**Common methods:**

* ``setConfirmable(bool)`` — CON (default) or NON for the next request(s).
* ``setContentFormat(format)`` — IANA Content-Format for the next request payload.
  Default ``OT_COAP_FORMAT_TEXT`` (0); omitted when the request has no body.
  See `Content-Format codes`_ below for the full ESP-IDF list.
* ``setPort(uint16_t)`` — destination port (default 5683).
* ``setTimeout(ms)`` — maximum time to wait for a response, in **milliseconds**
  (``5000`` = 5 s — same unit as ``HTTPClient::setTimeout()`` and OpenThread's
  ACK timeout). For confirmable plain CoAP requests, CON retransmission is tuned
  to match, so ``OT_COAP_ERROR_TIMEOUT`` means no answer within this limit.
  Values below ``OT_COAP_MIN_ALIGNED_TIMEOUT_MS`` (default **1500** ms =
  ``OT_COAP_MIN_ACK_TIMEOUT_MS`` × 1.5) are raised automatically. Call
  ``useDefaultCoapRetransmit()`` **before** ``setTimeout()`` if you need a
  shorter sketch wait with RFC wire retries. CoAPS: sketch cap only (see below).
* ``useDefaultCoapRetransmit()`` — optional: use RFC 7252 fixed wire retries
  (~93 s) instead of aligning with ``setTimeout()``. The sketch still blocks
  only for ``setTimeout(ms)``; OT may keep retrying in the background after
  timeout. No minimum ``setTimeout()`` is enforced in this mode.
* ``GET/PUT/POST/DELETE`` — return response code (≥ 0) or negative error.
* ``sendNonBlocking(host, method, path, payload)`` — fire-and-forget NON request;
  returns ``true`` once queued, never blocks.
* ``getString()``, ``readPayload()``, ``responseCode()``, ``remoteIP()``.

.. note::

   **Lazy UDP service.** The first plain client request starts the shared CoAP UDP
   socket (port 5683 by default) if the server has not already. It stays active until
   ``OThread.end()`` (or until the last server/client holder releases it). There is no
   separate client ``close()`` — tear down the stack or stop ``OThreadCoAPServer`` when
   you are done.

.. note::

   **Multicast is always NON.** RFC 7252 §8.1 forbids confirmable multicast, so when
   the destination is an IPv6 multicast address (first byte ``0xFF``) requests are
   sent NON regardless of ``setConfirmable()``. §8.2 also discourages replying to a
   group request, so for group commands use ``sendNonBlocking()`` (fire-and-forget)
   and let the server skip its reply. The blocking ``GET/PUT/POST/DELETE`` to a group
   still works if a server chooses to answer (they return the first reply), but that
   does not scale past one responder — address a single server by its unicast address
   when you need a confirmed response. The receiving server must join the group with
   ``joinMulticastGroup()``.

OThreadCoAPServer (global singleton)
------------------------------------

WebServer-style resource server. Handlers run when a matching request arrives
(on the OpenThread task); call ``resp.send()`` before returning. No ``loop()``
polling is required.

**Like ``OThread`` or ``Wi-Fi``:** OpenThread binds **one plain CoAP UDP service**
per device (default port **5683**). The library exposes a single global
``OThreadCoAPServer`` — do **not** construct ``OThreadCoAPServerClass`` or declare
a local server variable. Register many URI paths on that global with ``on()``,
``serve()``, and ``OThreadCoAPResourceStore``. Multiple ``OThreadCoAPClient``
instances are fine. ``OThreadCoAPSecureServer`` (5684) may run on the same device;
see ``CoAP_Greenhouse``.

.. code-block:: arduino

    static void onHello(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
        resp.setCode(OT_COAP_RESP_OK);
        resp.setPayload("Hello from CoAP!");
        resp.send();
    }

    OThreadCoAPServer.on("hello", OT_COAP_METHOD_GET, onHello);
    OThreadCoAPServer.begin();

``OThreadCoAP::plainServer()`` returns the same object as ``OThreadCoAPServer``.

**Common methods:**

* ``on(path, methodMask, handler, context)`` — register one handler per URI path.
  Pass ``nullptr`` for ``handler`` to **unregister** a path (Arduino ``WebServer`` uses
  a separate ``removeRoute()``; CoAP keeps registration on ``on()``). ``onNotFound(nullptr)``
  disables the default handler.
* ``onNotFound(handler)`` — default handler for unknown paths.
* ``serve(path, String*)`` — read-only / read-write ``String`` resource sugar.
  Multicast requests are not answered (RFC 7252 §8.2), same as ``on()`` handlers.
* ``setResourceValue(path, value)`` — update a ``serve()``-backed value.
* ``joinMulticastGroup(group)`` / ``leaveMulticastGroup(group)`` — receive requests
  sent to an IPv6 multicast group. ``joinMulticastGroup()`` requires a successful
  ``begin()`` first (returns ``false`` otherwise). Groups are left automatically on
  ``stop()``, including after a failed ``stop()`` that cleared ``running``.
  Full guide: :doc:`Multicast guide <Multicasting>`.

.. note::

   **One UDP port per device.** OpenThread exposes a single plain CoAP UDP bind per
   instance (default **5683**). ``OThreadCoAPServer`` and ``OThreadCoAPClient`` share it;
   a client using ``setPort()`` to a non-default port will fail if the server already
   holds the socket on another port.

Keep handlers short — they run on the OpenThread worker task, not in ``loop()``.

To serve a multicast resource, join the group after ``begin()``:

.. code-block:: arduino

    const IPAddress LAMP_GROUP(IPv6, lampGroupBytes);  // ff03::abcd
    OThreadCoAPServer.on("Lamp", OT_COAP_METHOD_GET | OT_COAP_METHOD_PUT, onLamp);
    OThreadCoAPServer.begin();
    OThreadCoAPServer.joinMulticastGroup(LAMP_GROUP);

Handler arguments:

* ``OThreadCoAPRequest`` — ``path()``, ``method()``, ``payloadString()``,
  ``isConfirmable()``, ``isMulticast()``, ``remoteIP()``.
* ``OThreadCoAPResponse`` — ``setCode()``, ``setPayload()``, ``setContentFormat()``,
  ``setLocation(path)`` (e.g. ``201 Created`` + ``Location-Path``), ``setMaxAge(seconds)``,
  ``send()``.

.. note::

   Multicast requests are **not auto-answered** (RFC 7252 §8.2). Check
   ``req.isMulticast()`` and skip ``resp.send()`` for group commands so that many
   servers sharing a group do not all reply to the same datagram. An explicit
   ``resp.send()`` is still honored if you deliberately want to respond.

OThreadCoAPSecureClient / OThreadCoAPSecureServer
-------------------------------------------------

CoAP over DTLS. Connect once, then issue path-only requests (client) or
register handlers (server). Requires ``OThreadCoAP::secureApiEnabled() == true``
at runtime (see build requirements above); otherwise secure classes return
``OT_COAP_ERROR_NOT_CONNECTED`` from ``begin()`` / ``connect()``.

**One CoAPS session (client):** only one ``OThreadCoAPSecureClient`` may be
``connect()``\ ed at a time. A failed ``connect()`` (timeout, handshake error, or
OT error) clears the session slot so the same client may retry.

**Server vs client on one device:** do **not** run ``OThreadCoAPSecureServer.begin()``
and ``OThreadCoAPSecureClient.connect()`` on the same node — OpenThread shares one
CoAPS stack and credential set per instance. Each direction is rejected with a serial
warning when the other role is active. Use plain ``OThreadCoAPServer`` plus
``OThreadCoAPSecureServer`` on a gateway and ``OThreadCoAPSecureClient`` on a peer
(``CoAP_Greenhouse`` / ``CoAP_Secure``).

If ``OThreadCoAPSecureServer.stop()`` fails to acquire the OpenThread lock, the
wrapper resets but CoAPS may still be active in the stack. In that state
``OThreadCoAPSecureClient.connect()`` remains blocked until you call
``OThreadCoAPSecureServer.begin()`` again (server recovery) or ``OThread.end()``.

The same lock-failure pattern applies to **plain** ``OThreadCoAPServer.stop()``:
the wrapper clears ``running`` but the CoAP UDP service may still be held in the
stack. Call ``OThreadCoAPServer.begin()`` again to recover, or ``OThread.end()``.
If ``OThread.getInstance()`` is unavailable during ``stop()`` (e.g. stack not
initialized), wrapper state is reset but service-held flags are preserved so a
later ``begin()`` can retry.

``OThreadCoAPServer.begin()`` and ``OThreadCoAPSecureServer.begin()`` return
``bool``. Use ``if (OThreadCoAPServer)`` (``operator bool()``) to test whether
the server wrapper considers itself running. If an idempotent ``begin()`` fails
while the server is already running (resync / attach error), ``running`` is cleared,
the plain UDP or CoAPS service is released, and ``operator bool()`` matches the
return value. After ``OThread.end()``, the next ``begin()`` uses the fresh-start
path (not resync).

**CoAPS server global:** use the library singleton ``OThreadCoAPSecureServer``
(do not construct ``OThreadCoAPSecureServerClass``). Register many paths with
``on()`` — pass ``nullptr`` for ``handler`` to unregister a path (same API as
``OThreadCoAPServer``). ``OThreadCoAP::secureServer()`` returns the same instance.

.. code-block:: arduino

    OThreadCoAPSecureClient client;
    client.setPSK(psk, pskLen, "client1");
    client.connect(serverIp);
    client.setConfirmable(true);
    client.setContentFormat(OT_COAP_FORMAT_JSON);
    int code = client.PUT("fan/speed", "{\"speed\":75}");
    client.disconnect();

    OThreadCoAPSecureServer.setPSK(psk, pskLen, "server1");
    OThreadCoAPSecureServer.on("status", OT_COAP_METHOD_GET, onStatus);
    OThreadCoAPSecureServer.begin();

``setTimeout()`` on the secure client caps how long blocking methods wait in the
sketch only; OpenThread does not expose per-request TX tuning for CoAPS, so
confirmable requests may keep retrying on the wire after
``OT_COAP_ERROR_TIMEOUT``. ``setConnectTimeout(ms)`` caps the DTLS handshake wait
during ``connect()``. ``onConnectEvent(callback, context)`` fires on connect /
disconnect events; ``connected()`` and ``isConnectionActive()`` report session state.

Plain and secure servers can run **together** on one device (5683 + 5684);
register ``on()`` on each server separately. Reuse the same handler function on
both servers when the resource logic is identical.

**``OThreadCoAPSecureServer`` scope (plain-only helpers):** the secure server
currently exposes credential setup, ``begin()`` / ``stop()``, and ``on()`` only.
These ``OThreadCoAPServer`` conveniences are **not** on the secure class:

* ``onNotFound()`` — unknown-path handler.
* ``serve()`` / ``setResourceValue()`` — ``String`` resource sugar.
* ``joinMulticastGroup()`` / ``leaveMulticastGroup()`` — multicast group join.

For those patterns on a dual-transport device, use ``OThreadCoAPServer`` on port
5683 (telemetry / multicast) and ``OThreadCoAPSecureServer`` on 5684 (commands).
Register the same C handler on both with separate ``on()`` calls, or use
``OThread.subscribeMulticast()`` at the stack level if you need multicast receive
outside the plain CoAP server helper. See :doc:`Multicast guide <Multicasting>`.

OThreadCoAPResourceStore
------------------------

REST collection helper for CRUD under a base path on **`OThreadCoAPServer` only**
(not ``OThreadCoAPSecureServer``):

.. code-block:: arduino

    OThreadCoAPResourceStore notes;
    OThreadCoAPServer.begin();
    notes.attach(OThreadCoAPServer, "notes", 8);

Supports ``GET notes`` (list), ``GET notes/<id>``, ``POST notes``,
``PUT notes/<id>``, ``DELETE notes/<id>``.

Multicast requests to the store path are **not answered** (RFC 7252 §8.2), matching
``on()`` handlers and the Native light-switch pattern.

JSON responses are capped at ``OT_COAP_MAX_PAYLOAD`` (default 512 bytes). If the
assembled body would exceed that limit, the store returns **5.00 Internal Server
Error** with a short JSON diagnostic instead of a truncated payload. Block-wise
transfer (RFC 7959) is not implemented in Phase 1 — keep collections small or
split data across items.

**Methods (CoAP + programmatic):**

* ``attach(server, basePath, maxItems)`` — register CRUD handlers on ``server``
  under ``basePath`` (plain server only). One store per server — returns
  ``false`` if another store is already attached.
* ``onChange(callback, context)`` — optional hook after any mutation (CoAP
  traffic **or** sketch calls to ``create()`` / ``update()`` / ``remove()``).
* ``count()``, ``getByIndex(index, out)``, ``getById(id, out)`` — read the
  in-memory collection from ``loop()`` or other tasks (not from CoAP handlers).
* ``create(body, assignedId)``, ``update(id, body)``, ``remove(id)`` — mutate
  the store from sketch code; CoAP clients see the same data.

.. code-block:: arduino

    notes.onChange([](const char *basePath, void *ctx) {
        (void)ctx;
        Serial.printf("Store '%s' changed (%u items)\n", basePath, (unsigned)notes.count());
    }, nullptr);

Build-time tunables
-------------------

Override on the compiler command line when needed:

+--------------------------------------+---------+--------------------------------------------------------------+
| Macro                                | Default | Meaning                                                      |
+======================================+=========+==============================================================+
| ``OT_COAP_DEFAULT_PORT``             | 5683    | Plain CoAP UDP port                                          |
| ``OT_COAP_MAX_PAYLOAD``              | 512     | Client response buffer size                                  |
| ``OT_COAP_MAX_RESOURCES``            | 4       | Max ``on()`` slots per server instance                       |
| ``OT_COAP_DEFAULT_TIMEOUT_MS``       | 5000    | Default for ``setTimeout()`` (ms); CON retries aligned       |
| ``OT_COAP_MIN_ACK_TIMEOUT_MS``       | 1000    | OpenThread minimum CON ACK spacing (``coap.h``)              |
| ``OT_COAP_MIN_ALIGNED_TIMEOUT_MS``   | 1500    | Floor for ``setTimeout()`` in aligned mode (ACK × 1.5)       |
+--------------------------------------+---------+--------------------------------------------------------------+

Response and error constants
----------------------------

Common response codes (HTTP-like ``class*100+detail``):

+-----------------------------------------+----------------------------------+
| Constant                                | ``responseCodeToString()``       |
+=========================================+==================================+
| ``OT_COAP_RESP_CREATED`` (201)          | 2.01 Created                     |
| ``OT_COAP_RESP_DELETED`` (202)          | 2.02 Deleted                     |
| ``OT_COAP_RESP_CHANGED`` (204)          | 2.04 Changed                     |
| ``OT_COAP_RESP_OK`` (205)               | 2.05 Content                     |
| ``OT_COAP_RESP_BAD_REQUEST`` (400)      | 4.00 Bad Request                 |
| ``OT_COAP_RESP_NOT_FOUND`` (404)        | 4.04 Not Found                   |
| ``OT_COAP_RESP_METHOD_NA`` (405)        | 4.05 Method Not Allowed          |
| ``OT_COAP_RESP_INTERNAL_ERROR`` (500)   | 5.00 Internal Server Error       |
+-----------------------------------------+----------------------------------+

Blocking client methods return a code from the table above (≥ 0) or a negative
``OT_COAP_ERROR_*`` value. Use ``OThreadCoAP::errorToString(code)`` in logs.

+-----------------------------------------+---------------------------+
| Constant                                | ``errorToString()``       |
+=========================================+===========================+
| ``OT_COAP_ERROR_TIMEOUT`` (-11)         | timeout                   |
| ``OT_COAP_ERROR_NO_RESPONSE`` (-12)     | no response               |
| ``OT_COAP_ERROR_NOT_ATTACHED`` (-13)    | not attached              |
| ``OT_COAP_ERROR_NO_BUFS`` (-14)         | no bufs                   |
| ``OT_COAP_ERROR_INVALID_ARGS`` (-15)    | invalid args              |
| ``OT_COAP_ERROR_NOT_CONNECTED`` (-16)   | not connected             |
| ``OT_COAP_ERROR_TLS_FAILED`` (-17)      | tls failed                |
| ``OT_COAP_ERROR_INVALID_STATE`` (-18)   | invalid state             |
+-----------------------------------------+---------------------------+

``OT_COAP_ERROR_NOT_ATTACHED`` means the device role is below Child.
``OT_COAP_ERROR_INVALID_STATE`` means the CoAP service or OT lock is not ready
(for example stack stopped, port bind conflict, or lazy client start failed) —
distinct from attachment.

Method masks for ``on()``:

* ``OT_COAP_METHOD_GET``, ``OT_COAP_METHOD_PUT``, ``OT_COAP_METHOD_POST``,
  ``OT_COAP_METHOD_DELETE`` — combine with ``|``.

Request method values (compare with ``req.method()``):

* ``OT_COAP_REQ_GET``, ``OT_COAP_REQ_PUT``, ``OT_COAP_REQ_POST``,
  ``OT_COAP_REQ_DELETE``.

Content-Format codes
--------------------

CoAP payloads carry an optional **Content-Format** option (RFC 7252 §12.3) with
an IANA-registered numeric code. Set it on **clients** with
``client.setContentFormat(code)`` before PUT/POST (ignored when there is no
payload). Set it on **responses** with ``resp.setContentFormat(code)`` before
``resp.send()``.

The Arduino wrapper defines ``OT_COAP_FORMAT_*`` constants that mirror
``otCoapOptionContentFormat`` in ESP-IDF ``coap.h``. Any ``uint16_t`` IANA code
is accepted; codes outside the table below still encode correctly on the wire.

+--------+--------------------------------------------+----------------------------------------------+
| Code   | Constant                                   | Media type                                   |
+========+============================================+==============================================+
| 0      | ``OT_COAP_FORMAT_TEXT``                    | text/plain; charset=utf-8                    |
| 16     | ``OT_COAP_FORMAT_COSE_ENCRYPT0``           | application/cose; cose-type=cose-encrypt0    |
| 17     | ``OT_COAP_FORMAT_COSE_MAC0``               | application/cose; cose-type=cose-mac0        |
| 18     | ``OT_COAP_FORMAT_COSE_SIGN1``              | application/cose; cose-type=cose-sign1       |
| 40     | ``OT_COAP_FORMAT_LINK``                    | application/link-format                      |
| 41     | ``OT_COAP_FORMAT_XML``                     | application/xml                              |
| 42     | ``OT_COAP_FORMAT_OCTET_STREAM``            | application/octet-stream                     |
| 47     | ``OT_COAP_FORMAT_EXI``                     | application/exi                              |
| 50     | ``OT_COAP_FORMAT_JSON``                    | application/json                             |
| 51     | ``OT_COAP_FORMAT_JSON_PATCH``              | application/json-patch+json                  |
| 52     | ``OT_COAP_FORMAT_MERGE_PATCH_JSON``        | application/merge-patch+json                 |
| 60     | ``OT_COAP_FORMAT_CBOR``                    | application/cbor                             |
| 61     | ``OT_COAP_FORMAT_CWT``                     | application/cwt                              |
| 96     | ``OT_COAP_FORMAT_COSE_ENCRYPT``            | application/cose; cose-type=cose-encrypt     |
| 97     | ``OT_COAP_FORMAT_COSE_MAC``                | application/cose; cose-type=cose-mac         |
| 98     | ``OT_COAP_FORMAT_COSE_SIGN``               | application/cose; cose-type=cose-sign        |
| 101    | ``OT_COAP_FORMAT_COSE_KEY``                | application/cose-key                         |
| 102    | ``OT_COAP_FORMAT_COSE_KEY_SET``            | application/cose-key-set                     |
| 110    | ``OT_COAP_FORMAT_SENML_JSON``              | application/senml+json                       |
| 111    | ``OT_COAP_FORMAT_SENSML_JSON``             | application/sensml+json                      |
| 112    | ``OT_COAP_FORMAT_SENML_CBOR``              | application/senml+cbor                       |
| 113    | ``OT_COAP_FORMAT_SENSML_CBOR``             | application/sensml+cbor                      |
| 114    | ``OT_COAP_FORMAT_SENML_EXI``               | application/senml-exi                        |
| 115    | ``OT_COAP_FORMAT_SENSML_EXI``              | application/sensml-exi                       |
| 256    | ``OT_COAP_FORMAT_COAP_GROUP_JSON``         | application/coap-group+json                  |
| 310    | ``OT_COAP_FORMAT_SENML_XML``               | application/senml+xml                        |
| 311    | ``OT_COAP_FORMAT_SENSML_XML``              | application/sensml+xml                       |
+--------+--------------------------------------------+----------------------------------------------+

See also the `IANA CoAP Content-Formats registry
<https://www.iana.org/assignments/core-parameters/core-parameters.xhtml#content-formats>`_.

**Examples in this library:**

* `CoAP_CRUD notes_client
  <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client>`_ —
  POST/PUT JSON bodies; client calls ``setContentFormat(OT_COAP_FORMAT_JSON)``;
  ``OThreadCoAPResourceStore`` answers with JSON via ``resp.setContentFormat()``.
* `CoAP_SimpleGet
  <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet>`_ —
  text response; server uses the default ``OT_COAP_FORMAT_TEXT``.
* `CoAP_Greenhouse
  <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse>`_ —
  actuator PUT payloads are plain decimal strings; default text/plain is correct
  (no ``setContentFormat()`` needed).

.. note::

   ``OT_COAP_RESP_*`` and ``OT_COAP_REQ_*`` are Arduino wrapper constants.
   OpenThread's C API uses ``OT_COAP_CODE_*`` in ``coap.h`` — do not redefine
   those names in sketches.

Native CoAP Examples
--------------------

These examples live in the `Native CoAP examples on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP>`_ (Native API,
not CLI). They replace the older `CLI CoAP demos on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP>`_ with
typed method calls.

+----------------------+--------------------------------------------------------+
| Folder               | Demonstrates                                           |
+======================+========================================================+
| CoAP_SimpleGet_      | Minimal server + client; one GET resource              |
| CoAP_Light_Switch_   | Multicast lamp control (CLI ``coap_lamp`` rewrite)     |
| CoAP_Sensor_         | ``serve()`` resource + periodic NON GET client         |
| CoAP_CRUD_           | ``OThreadCoAPResourceStore`` REST notes (JSON Format)  |
| CoAP_Secure_         | CoAPS with PSK (requires secure build flag)            |
| CoAP_Greenhouse_     | Plain + CoAPS; NON telemetry + CON actuators           |
+----------------------+--------------------------------------------------------+

.. _CoAP_SimpleGet: https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet
.. _CoAP_Light_Switch: https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch
.. _CoAP_Sensor: https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor
.. _CoAP_CRUD: https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD
.. _CoAP_Secure: https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure
.. _CoAP_Greenhouse: https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse

See also the Native examples overview:
`Native README on GitHub <https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md>`_ and
`CoAP README on GitHub <https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md>`_.

Best Practices
--------------

* **Singleton servers**: like ``WebServer`` on one port — use the globals
  ``OThreadCoAPServer`` and ``OThreadCoAPSecureServer``, many ``on()`` paths each.
  Do not construct ``OThreadCoAPServerClass`` / ``OThreadCoAPSecureServerClass``.
* **Attach before CoAP traffic**: call ``OThreadCoAPServer.begin()`` and run client requests
  only after the device is attached (role ≥ Child).
* **After ``OThread.end()``**: call ``OThreadCoAPServer.begin()`` (and
  ``OThreadCoAPSecureServer.begin()`` if used) again on the new stack instance.
* **CoAPS role split**: ``OThreadCoAPSecureServer.begin()`` and
  ``OThreadCoAPSecureClient.connect()`` must not run on the same device (each API
  rejects the other while active).
* **One path, one handler**: OpenThread matches the first resource with an exact
  URI path; multiplex methods with ``methodMask`` and ``switch (req.method())``.
* **CON vs NON**: see :ref:`coap_con_non` and :ref:`coap_multicast_con` for
  protocol semantics, multicast/CON interaction, ``setConfirmable()``, timeouts,
  and example mapping.
* **Security split**: expose telemetry on plain CoAP; restrict commands to CoAPS
  when credentials matter.
* **Resource slots**: each server instance supports up to ``OT_COAP_MAX_RESOURCES``
  (default **4**) ``on()`` paths. Exceeding the limit makes ``on()`` fail; unregister
  unused paths with ``on(path, mask, nullptr)`` or raise the limit at build time.
* **One UDP port (5683)**: plain ``OThreadCoAPServer`` and ``OThreadCoAPClient``
  share a single CoAP UDP bind per device (library macro ``OT_COAP_DEFAULT_PORT``).
  A plain client lazily starts the service on the first send if no server has
  ``begin()`` yet — call ``server.begin()`` first if you need a custom listen port.
  When the last ``OThreadCoAPClient`` object is destroyed, the client lazy-start
  hold on the shared service is released if no server still needs it. CoAPS uses
  port **5684** when enabled.
* **Do not use port 61631** for application resources — it is reserved for
  Thread internal TMF CoAP.

Advanced options (not in every example sketch)
----------------------------------------------

.. code-block:: arduino

    // Custom listen port — call before begin(); prefer server.begin() before clients
    OThreadCoAPServer.setPort(5683);
    OThreadCoAPServer.begin();

    // RFC 7252 default retransmit profile before a short client timeout
    OThreadCoAPClient client;
    client.useDefaultCoapRetransmit();
    client.setTimeout(3000);

    // Drop multicast membership at runtime (pair with joinMulticastGroup)
    OThreadCoAPServer.leaveMulticastGroup(IPAddress("ff03::abcd"));

See `README.md <https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/README.md>`_ and
``CoAP_Light_Switch/light/`` for ``joinMulticastGroup()`` in context.
