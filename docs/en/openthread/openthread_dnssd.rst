##################
OThreadDNSSD Class
##################

About
-----

The ``OThreadDNSSD`` class provides an Arduino-style **DNS-SD** API for Thread
(similar to Wi-Fi ``MDNS`` / ESPmDNS in shape, but **not** mDNS on the mesh):

* **Advertise** — register a hostname and services via the OpenThread **SRP client**
  with an SRP server (typical OTBR).
* **Discover** — browse/resolve via the OpenThread **DNS client** (same BR DNS/SRP
  server when auto-selected from Network Data).

External hosts can also discover advertised services through the Border Router's
DNS-SD / Advertising Proxy.

This is **not** OpenThread CLI ``mdns`` (infrastructure-link multicast DNS on a
Border Router) and **not** Wi-Fi ``ESPmDNS``.

The global instance is ``OThreadDNSSD``.

**Key Features:**

* Familiar API: ``begin``, ``addService``, ``addServiceTxt``, ``end``,
  ``queryService``, ``queryHost`` (like ESPmDNS).
* Auto host address and SRP auto-start (finds the Border Router SRP server;
  DNS default server follows when DNS client is enabled).
* ``waitForAnnounce()`` / ``onServiceEvent()`` because registration is asynchronous.
* ``startQueryService()`` / ``startQueryHost()`` / ``onQueryEvent()`` for non-blocking discover
  (blocking ``queryService`` / ``queryHost`` remain for ESPmDNS parity).
* Fixed-size service / TXT / query-result pools (same idea as ``OThreadScan``).

**Prerequisites:**

* ``OThread.begin()``, set the OTBR **Network Key** on a ``DataSet``, then
  ``networkInterfaceUp()``, ``start()``, and an attached role (child / router /
  leader). Channel, PAN ID, and other fields are not required in the sketch.
* A Thread network that publishes **SRP** (advertise) and **DNS** (discover) in
  Network Data (typical OTBR). Without SRP, ``waitForAnnounce()`` times out.
  Without DNS, ``queryService`` / ``queryHost`` fail or return empty.

Include the header:

.. code-block:: arduino

    #include <OThreadDNSSD.h>

Requires ``CONFIG_OPENTHREAD_SRP_CLIENT`` (advertise). Discover APIs also need
``CONFIG_OPENTHREAD_DNS_CLIENT``. Both are enabled by default in Arduino
OpenThread FTD/MTD builds.

sdkconfig capabilities
----------------------

.. list-table::
   :header-rows: 1
   :widths: 22 38 20 20

   * - Capability
     - Device sdkconfig
     - Arduino default
     - Network need
   * - Advertise
     - ``CONFIG_OPENTHREAD_SRP_CLIENT``
     - Enabled
     - OTBR **SRP server**
   * - Discover
     - ``CONFIG_OPENTHREAD_DNS_CLIENT``
     - Enabled
     - OTBR **DNS/SRP**
   * - On-device SRP server
     - ``CONFIG_OPENTHREAD_BORDER_ROUTER``
     - Not enabled
     - Second board can advertise to it

Compared to Wi-Fi MDNS
----------------------

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Wi-Fi ``MDNS``
     - Thread ``OThreadDNSSD``
   * - ``MDNS.begin("esp32")``
     - ``OThreadDNSSD.begin("sensor-1")``
   * - ``MDNS.addService(...)``
     - ``OThreadDNSSD.addService(...)``
   * - ``MDNS.addServiceTxt(...)``
     - ``OThreadDNSSD.addServiceTxt(...)``
   * - ``MDNS.queryService`` / ``queryHost``
     - ``OThreadDNSSD.queryService`` / ``queryHost``
   * - Immediate local multicast
     - Async SRP register / DNS query → Border Router
   * - —
     - ``waitForAnnounce(timeoutMs)``

Quick start (advertise)
-----------------------

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadDNSSD.h>

    void setup() {
      Serial.begin(115200);
      OThread.begin(false);
      // Set Network Key to match the OTBR; other dataset fields are learned on attach.
      DataSet ds;
      ds.clear();
      ds.setNetworkKey(/* OTBR network key */);
      OThread.commitDataSet(ds);
      OThread.networkInterfaceUp();
      OThread.start();
      // ... wait until attached ...

      if (!OThreadDNSSD.begin("sensor-1")) {
        Serial.println("OThreadDNSSD.begin failed");
        while (true) {
          delay(1000);  // do not return from setup() — loop() would still run
        }
      }
      OThreadDNSSD.addService("ot", "udp", 12345);
      OThreadDNSSD.addServiceTxt("ot", "udp", "path", "/status");

      if (OThreadDNSSD.waitForAnnounce(30000)) {
        Serial.println("Service announced");
      } else {
        Serial.println("Announce timed out (no SRP server yet?) — keep polling");
      }
    }

    void loop() {
      delay(5000);
      Serial.printf("announceComplete=%d\r\n", OThreadDNSSD.isAnnounceComplete());
    }

Quick start (discover)
----------------------

.. code-block:: arduino

    if (!OThreadDNSSD.begin("browser")) {
      Serial.println("OThreadDNSSD.begin failed");
      while (true) {
        delay(1000);  // do not return from setup() — loop() would still run
      }
    }
    int n = OThreadDNSSD.queryService("ot", "udp");
    for (int i = 0; i < n; i++) {
      Serial.println(OThreadDNSSD.instanceName(i));
      Serial.println(OThreadDNSSD.address(i).toString());
      Serial.println(OThreadDNSSD.port(i));
    }
    IPAddress a = OThreadDNSSD.queryHost("sensor-1");
    Serial.println(a.toString());

Pair a second board running ``ThreadDNSSD_Advertise`` on the same Network Key.
A failed ``begin()`` is local/config — do not call query APIs until it succeeds.
Arduino ``return`` from ``setup()`` still runs ``loop()``; see the Native
ThreadDNSSD examples (simple sketches halt; ``ThreadDNSSD_Advertise_Callback``
and ``ThreadDNSSD_UDP_Light`` retry ``begin()`` from ``loop()``).

API summary
-----------

* ``bool begin(const char *hostName)`` — configure host, auto addresses, auto-start.
  Prefer a per-device unique label when several nodes share one OTBR.
  Discover-only sketches also register this host (SRP auto-start selects the DNS server).
  ``false`` is a local/config failure, not "SRP server not ready". Do not call
  advertise/query APIs until a later successful ``begin()``.
* ``const char *hostname()`` — local host label from a successful ``begin()``
  (empty if not started). After a browse, ``hostname(i)`` is the **discovered**
  host at index ``i``.
* ``void end()`` — unregister locally and stop the SRP client (also called from
  ``OThread.end()``). If an async discover is in flight, ``onQueryEvent`` gets
  ``OT_DNSSD_QUERY_ERROR`` / ``OT_ERROR_ABORT`` first (caller task), then
  ``OT_DNSSD_EVENT_REMOVED``. If the OpenThread lock cannot be acquired, SRP
  unregister toward the server may not run; local state is still cleared and
  ``REMOVED`` is still delivered.
* ``bool started()`` — true after a successful ``begin()`` and before ``end()``.
* ``void setInstanceName(const char *name)`` — instance label for services
  (default = host name).
* ``bool addService(service, proto, port)`` — e.g. ``"ot", "udp", 12345``.
  Leading underscores are stripped; empty labels after strip are rejected.
  Same service+proto updates in place (idempotent) unless a remove of that
  slot is still in flight.
* ``bool addServiceTxt(service, proto, key, value)`` — TXT record on an existing service.
  Values are C strings; empty value is a boolean key. Embedded NUL is not supported
  (discover ``txt`` / ``txtKey`` also return ``String``).
* ``bool addServiceSubtype(service, proto, subtype)`` — optional DNS-SD subtype
  (leading underscores stripped; duplicates are idempotent).
* ``bool removeService(service, proto)`` — remove one service. The slot is not
  reusable until OpenThread reports it removed.
* ``void onServiceEvent(callback, context)`` — ``OT_DNSSD_EVENT_ANNOUNCED`` /
  ``REMOVED`` / ``ERROR``. One SRP callback delivers at most one of these.
  ``REMOVED`` means the host or the last local service is gone (or ``end()``),
  not a partial service delete. Do **not** call other ``OThreadDNSSD`` methods from the
  callback.
* ``bool isAnnounceComplete()`` — live read of local SRP client item state
  (host + services ``Registered``). Reflects OpenThread as soon as the server
  accepts the update; not a direct probe of the Border Router.
* ``bool waitForAnnounce(timeoutMs)`` — success, terminal error, or timeout.
  ``OT_ERROR_DUPLICATED`` / ``OT_ERROR_SECURITY`` end the wait with ``false``
  (library does not rename); the sketch chooses the next action. Transient
  errors keep waiting until the timeout. Timeout is not terminal — poll
  ``isAnnounceComplete()`` if OpenThread may still reach ``Registered`` later
  (same after a name-conflict fail if the OTBR later frees the name).
* ``otError lastError()`` — last SRP or DNS callback error.
* ``int queryService(service, proto)`` — DNS browse; fills a fixed result pool
  (needs ``CONFIG_OPENTHREAD_DNS_CLIENT``).
* ``IPAddress queryHost(host, timeoutMs)`` — resolve host AAAA
  (default ``OT_DNSSD_QUERY_TIMEOUT_MS``; that default is also raised to the
  OpenThread DNS client wait if longer. An explicit timeout is not changed).
* ``void onQueryEvent(callback, context)`` — async discover events
  (``OT_DNSSD_QUERY_DONE`` / ``OT_DNSSD_QUERY_ERROR``).
* ``bool startQueryService(service, proto)`` / ``bool startQueryHost(host)`` —
  non-blocking; completion via ``onQueryEvent``. One discover in flight at a time.
* ``bool isQueryInProgress()``, ``int queryResultCount()``, ``IPAddress resolvedAddress()``.
* After ``queryService`` / ``startQueryService``: ``instanceName(i)``, ``hostname(i)``, ``address(i)``,
  ``port(i)``, ``numTxt(i)``, ``hasTxt`` / ``txt`` / ``txtKey`` (ESPmDNS-style
  indexed getters into the fixed result pool).
  An empty browse (OTBR up, zero matching instances) returns ``0`` with
  ``lastError == OT_ERROR_NONE`` once the DNS/Discovery Proxy reply arrives
  (often several seconds; not a failure timeout).

**Discover results lifetime.** After a discover **timeout**, ``end()``, or before the
next ``queryService`` / ``queryHost`` / ``startQuery*`` call, do **not** rely on
result getters (``queryResultCount()``, ``instanceName(i)``, ``resolvedAddress()``,
etc.) as stable. An in-flight OpenThread DNS callback may still complete; the
library invalidates that operation (each DNS request carries the generation
from dispatch, so a late OpenThread callback cannot apply to a **new** query),
but sketches should copy what they need when the call returns (or on
``OT_DNSSD_QUERY_DONE``) and start a fresh query for updated data. Prefer
``lastError()`` to distinguish timeout (``OT_ERROR_RESPONSE_TIMEOUT``) from an
empty successful browse (``OT_ERROR_NONE`` with count ``0``).

Fixed memory caps
-----------------

Override **before** ``#include <OThreadDNSSD.h>``:

**Pools** (indexed with ``uint8_t``; each must be in ``1..255`` or the
header fails to compile):

* ``OT_DNSSD_MAX_SERVICES`` (default 5)
* ``OT_DNSSD_MAX_TXT_ENTRIES`` (default 4 per service)
* ``OT_DNSSD_MAX_SUBTYPES`` (default 4 per service)
* ``OT_DNSSD_MAX_QUERY_RESULTS`` (default 16)
* ``OT_DNSSD_MAX_DNS_CB_CTX`` (default 16) — in-flight OpenThread DNS callback
  contexts. A slot stays reserved until OpenThread invokes the callback,
  including after a local query timeout or ``end()``. Increase this if a sketch
  issues many short ``queryHost`` timeouts before those callbacks fire.

**Timeouts:**

* ``OT_DNSSD_QUERY_TIMEOUT_MS`` (default 10000 for browse and for the
  ``queryHost`` default). Browse always waits at least this long, and also
  at least the OpenThread DNS client wait + 1 s. ``queryHost`` applies that
  same floor only when the caller uses the default; an explicit timeout is
  honored as-is.

**Name / TXT lengths:**

* ``OT_DNSSD_HOST_NAME_MAX``, ``OT_DNSSD_INSTANCE_NAME_MAX``
* ``OT_DNSSD_LABEL_MAX`` (default 15) — max length of each ``service`` /
  ``proto`` input (including any leading underscores). All leading ``_``
  characters are then stripped; the remaining label must be non-empty.
* ``OT_DNSSD_SERVICE_NAME_MAX`` — max length of the encoded ``_type._proto``
  string. Default is ``2 * OT_DNSSD_LABEL_MAX + 3`` so a full-length type and
  proto always fit. If you override either macro, keep
  ``OT_DNSSD_SERVICE_NAME_MAX >= 2 * OT_DNSSD_LABEL_MAX + 3`` (the header
  ``#error``\ s otherwise). Do not keep an older literal ``32`` when
  ``OT_DNSSD_LABEL_MAX`` is ``15``.
* ``OT_DNSSD_TXT_KEY_MAX``, ``OT_DNSSD_TXT_VALUE_MAX``, ``OT_DNSSD_SUBTYPE_MAX``

Full pools return ``false`` / capped counts (fail closed). Strings are copied into
fixed slots; sketch temporaries are never stored as pointers for OpenThread.

Examples
--------

Native examples under ``libraries/OpenThread/examples/Native/ThreadDNSSD/``:

* ``ThreadDNSSD_Advertise`` — simple blocking ``waitForAnnounce``
* ``ThreadDNSSD_Advertise_Callback`` — ``onServiceEvent`` and re-advertise recovery
* ``ThreadDNSSD_Remove`` — two ``addService`` / ``removeService`` cycles, then ``end()``
* ``ThreadDNSSD_Query`` — browse ``_ot._udp`` (pair with Advertise)
* ``ThreadDNSSD_QueryHost`` — resolve a host label (pair with Advertise)
* ``ThreadDNSSD_Query_Callback`` — async ``startQueryService`` then ``startQueryHost`` via ``onQueryEvent``
* ``ThreadDNSSD_UDP_Light`` — application lab: Thread light + switch + Wi-Fi web UI
  (``_otlight._udp`` over UDP; light re-advertises after OTBR restart / lost attach;
  Wi-Fi discovers the light via OTBR Advertising Proxy mDNS)

See each example README in that folder for OTBR lab setup and expected Serial
output. Related overview: :doc:`openthread`.

Lab situations (OTBR + flash / reset)
-------------------------------------

SRP host and service names are owned by an **ECDSA key** that OpenThread stores in
**NVS**. The Border Router remembers that ownership for a long **key-lease** (often
many days). Registrations on a typical OTBR are also **soft state** (often
in-memory only). Discover results disappear after an OTBR restart until nodes
advertise again.

**Check what the OTBR currently has** (OTBR OpenThread CLI / ``ot-ctl``):

.. code-block:: text

    srp server service
    srp server host

Example lines include the instance (e.g. ``sensor-1._ot._udp...``), ``deleted:``,
``port:``, ``host:``, and ``addresses:``. There is **no** stock ``srp server``
command to delete another device's registration; removal is done by the
**client** that owns the key (``OThreadDNSSD.end()`` / ``ThreadDNSSD_Remove``), or by
clearing the OTBR's in-memory database (see reset below).

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Situation
     - What usually happens
   * - Arduino IDE **Erase Flash → Sketch Only**, then upload
     - NVS (SRP key) kept. Same hostname can re-register successfully.
   * - Arduino IDE **Erase All Flash** (or any erase that clears NVS)
     - NVS wiped → **new** SRP key. OTBR may still hold the old hostname →
       ``OT_ERROR_DUPLICATED``. Use a unique name, wait for key-lease expiry, or
       reset OTBR.
   * - **SoC reset** (EN / power cycle, no full flash erase)
     - NVS kept. SRP client can refresh the same names with the same key.
   * - **OTBR / otbr-agent reset**
     - SRP service list often becomes empty. Device must **advertise again**
       (see ``ThreadDNSSD_Advertise_Callback`` and ``ThreadDNSSD_UDP_Light``
       recovery). If a prior
       ``OT_ERROR_DUPLICATED`` wait already failed, OpenThread may still retry
       once the name is free — poll ``isAnnounceComplete()`` (live local SRP
       ``Registered`` state). Discover boards see empty results until re-advertise.
   * - **Another board, same hostname**
     - ``OT_ERROR_DUPLICATED``. Prefer unique hostnames (e.g. include MAC).
   * - ``OThreadDNSSD.end()`` on the registering device
     - Client asks the server to remove host + services (including key lease when
       possible). Confirm with ``srp server service``.

The library **reports** name conflicts; it does **not** rename the host for you.
Sketches decide whether to pick a new name or stop. ``isAnnounceComplete()``
reads the local OpenThread SRP client item state on each call (not a sticky
callback-only flag, and not a direct query of the Border Router).

Advanced notes
--------------

Under the hood this uses OpenThread ``otSrpClient*`` (advertise) and
``otDnsClient*`` (discover). DNS default server follows the selected SRP server
when auto-start is enabled. Lease intervals and manual server addresses are not
exposed in the beginner API.

See also: :doc:`openthread`, :doc:`openthread_core`.
