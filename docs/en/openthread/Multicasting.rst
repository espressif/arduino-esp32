########################
IPv6 Multicast on Thread
########################

About
-----

This page explains how to **join and leave IPv6 multicast groups** on the Thread
interface with the OpenThread Arduino library: stack-level APIs, UDP and CoAP
wrappers, timing, shutdown, and which shipped examples use each pattern.

Multicast lets one sender reach many receivers on the mesh without listing each
node's unicast address. Application demos in this tree often use **realm-local**
groups (``ff03::/16``) so every node in the partition can participate.

Send vs receive
---------------

.. list-table::
   :header-rows: 1
   :widths: 28 18 54

   * - Role
     - Subscribe?
     - Typical API
   * - **Receiver** — must accept packets to a group address
     - **Yes**
     - ``subscribeMulticast()``, ``beginMulticast()``, or ``joinMulticastGroup()``
   * - **Sender** — only transmits to a group address
     - **No**
     - UDP ``beginPacket(group, port)`` / ``endPacket()`` or CoAP client send

Sending to ``ff03::abcd`` does not require joining that group. Only nodes that
**listen** for group traffic must subscribe.

Stack-level API (``OThread``)
-----------------------------

These methods call OpenThread ``otIp6SubscribeMulticastAddress()`` /
``otIp6UnsubscribeMulticastAddress()``:

.. code-block:: arduino

    bool subscribeMulticast(const IPAddress &group);
    bool unsubscribeMulticast(const IPAddress &group);

* ``group`` must be an **IPv6 multicast** address (first byte ``0xFF``).
* Call on the global ``OThread`` singleton after ``OThread.begin()``.
* **Best practice:** subscribe after **attach** (role Child, Router, or Leader).
* Pair every subscribe with unsubscribe before shutdown, or use a wrapper that
  unsubscribes in ``stop()``.

See also :doc:`openthread_core` (``subscribeMulticast`` / ``unsubscribeMulticast``).

Reference counting
******************

Membership is **reference-counted**:

.. list-table::
   :widths: 45 55

   * - First ``subscribeMulticast(g)``
     - OpenThread joins ``g``; refcount = 1
   * - Another subscribe to the same ``g``
     - Refcount only
   * - ``unsubscribeMulticast(g)`` while refcount > 1
     - Refcount only
   * - Last unsubscribe for ``g``
     - OpenThread leaves ``g``

``OThreadUDP.beginMulticast()`` and ``OThreadCoAPServer.joinMulticastGroup()``
can share the same group. The refcount table is mutex-protected.

Query membership with ``getMulticastAddressCount()``, ``getMulticastAddress()``,
``getAllMulticastAddresses()``, and ``clearMulticastAddressCache()`` when the role
or membership changes.

UDP: ``OThreadUDP``
-------------------

``beginMulticast(group, port)`` (UDP receivers)
***********************************************

.. code-block:: arduino

    OThreadUDP Udp;
    const uint8_t groupBytes[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xab, 0xcd};
    IPAddress group(IPv6, groupBytes);  // ff03::abcd

    Udp.beginMulticast(group, 5051);
    // parsePacket() / read() in loop()
    Udp.stop();  // unsubscribes and closes the socket

Internally: ``begin(port)`` then ``OThread.subscribeMulticast(group)``.
``stop()`` calls ``unsubscribeMulticast()`` for that group. The socket also
receives **unicast** on the same port (for example ACK replies).

See :doc:`openthread_udp`.

``begin(port)`` only (senders and unicast receivers)
****************************************************

.. code-block:: arduino

    Udp.begin(5051);
    Udp.beginPacket(multicastGroup, 5051);
    Udp.write(...);
    Udp.endPacket();

Use on **clients** that send to a multicast group but do not receive group
traffic. Bind a local port when expecting **unicast** replies.

Manual subscribe + bind
***********************

.. code-block:: arduino

    Udp.begin(5050);
    OThread.subscribeMulticast(group);
    // ...
    OThread.unsubscribeMulticast(group);
    Udp.stop();

Prefer ``beginMulticast()`` unless sharing one subscription across several
sockets or toggling membership without closing the socket.

CoAP: ``OThreadCoAPServer``
---------------------------

Servers that **receive** multicast CoAP requests must join the group **after**
``begin()``:

.. code-block:: arduino

    OThreadCoAPServer.on("Lamp", OT_COAP_METHOD_GET | OT_COAP_METHOD_PUT, onLamp);
    OThreadCoAPServer.begin();
    OThreadCoAPServer.joinMulticastGroup(LAMP_GROUP);  // e.g. ff03::abcd

* ``joinMulticastGroup()`` → ``OThread.subscribeMulticast()``.
* ``leaveMulticastGroup(group)`` drops one reference at runtime.
* ``stop()`` leaves all groups recorded by the server.

CoAP multicast behavior (RFC 7252)
***********************************

* Clients: use **non-confirmable** (NON) multicast requests (§8.1);
  ``sendNonBlocking()`` for fire-and-forget.
* Servers: **do not reply** to multicast (§8.2); check ``req.isMulticast()``
  before ``resp.send()``.
* Unicast to one server: use blocking ``client.PUT(...)`` / ``GET(...)``.

Multicast and CON
*****************

**CON and multicast cannot be combined.** For destinations ``0xFF…``, the client
always sends **NON**, even if ``setConfirmable(true)`` (RFC 7252 §8.1). CON
retransmission and ``setTimeout()`` tuning **do not apply** to multicast sends.

.. list-table::
   :header-rows: 1
   :widths: 34 66

   * - Client call
     - Multicast behavior
   * - Blocking method + ``setConfirmable(true)``
     - Forced **NON**; may block for first reply only
   * - ``sendNonBlocking()``
     - **NON**, no wait (``CoAP_Light_Switch`` switch)
   * - Confirmed command to one device
     - **Unicast CON** to that node's address

Servers need ``joinMulticastGroup()`` to receive group CoAP; group requests are
NON — skip ``resp.send()`` when ``req.isMulticast()``. See :ref:`coap_con_non`
in :doc:`openthread_coap` for the full CON/NON guide.

``OThreadCoAPSecureServer`` has no ``joinMulticastGroup()``. Use plain
``OThreadCoAPServer`` on port **5683** for multicast, secure server on **5684**
for commands, or ``OThread.subscribeMulticast()`` at the stack level.

See :doc:`openthread_coap`.

CLI equivalent
--------------

CLI sketches join groups with:

.. code-block:: text

    ipmaddr add ff03::abcd

Equivalent to ``subscribeMulticast()`` for application traffic. Pair with
``udp bind`` in UDP examples.

Multicast scopes
----------------

.. list-table::
   :header-rows: 1
   :widths: 12 28 60

   * - Prefix
     - Scope
     - Examples in this library
   * - ``ff02::``
     - Link-local (one hop)
     - Rare in demos
   * - ``ff03::``
     - Realm-local (whole partition)
     - Native UDP/CoAP light switch (``ff03::abcd``)
   * - ``ff05::``
     - Site-local
     - CLI CoAP lamp/switch (``ff05::abcd``)

Align **network key, channel, group address, and scope** when mixing CLI and
Native boards. Built-in groups such as ``ff03::1`` appear in address listings;
application demos often use a custom suffix (for example ``ff03::abcd``).

Shutdown order
--------------

Before ``OThread.end()``:

1. ``OThreadCoAPServer.stop()`` / destroy CoAP clients
2. ``OThreadUDP.stop()`` (unsubscribes after ``beginMulticast()``)
3. Direct ``unsubscribeMulticast()`` for any remaining groups
4. ``OThread.end()``

See :doc:`openthread` (Shutdown Order) and the Native
`StackShutdown example <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown>`_.

Examples that use multicast
---------------------------

Native API
**********

.. list-table::
   :header-rows: 1
   :widths: 22 14 14 50

   * - Example
     - Sketch
     - Role
     - Mechanism
   * - `UDP Light Switch <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch>`_
     - ``light``
     - Receiver
     - ``OThreadUDP.beginMulticast(ff03::abcd, 5051)``
   * - `UDP Light Switch <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch>`_
     - ``switch``
     - Sender
     - ``Udp.begin(5051)`` + send to ``ff03::abcd`` (no subscribe)
   * - `CoAP Light Switch <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch>`_
     - ``light``
     - Receiver
     - ``OThreadCoAPServer.joinMulticastGroup(ff03::abcd)``
   * - `CoAP Light Switch <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch>`_
     - ``switch``
     - Sender
     - NON multicast ``PUT`` via ``sendNonBlocking()``

Native ``UDP_SensorNetwork`` uses **unicast** to the Leader RLOC, not application
multicast.

CLI API
*******

.. list-table::
   :header-rows: 1
   :widths: 22 18 14 46

   * - Example
     - Sketch
     - Role
     - Mechanism
   * - `UDP sensor collector <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/UDP/udp_sensor_collector>`_
     - collector
     - Receiver
     - ``ipmaddr add ff03::abcd`` + ``udp bind``
   * - `UDP sensor node <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/UDP/udp_sensor_node>`_
     - sensor
     - Sender
     - ``udp send ff03::abcd 5050 ...``
   * - `CoAP lamp <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_lamp>`_
     - lamp
     - Receiver
     - CLI multicast ``ff05::abcd``
   * - `CoAP switch <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch>`_
     - switch
     - Sender
     - CON CoAP PUT to ``ff05::abcd``

Documentation sample
********************

:doc:`openthread_udp` includes a minimal receiver with ``beginMulticast(ff03::1, 7)``.

Quick reference
---------------

.. code-block:: text

    Receiver (UDP)     →  Udp.beginMulticast(group, port)
    Receiver (CoAP)    →  CoAPServer.joinMulticastGroup(g)
    Sender             →  beginPacket(group, port) — no subscribe
    Stack / advanced   →  OThread.subscribeMulticast(g)
    Runtime leave      →  leaveMulticastGroup(g) or unsubscribeMulticast(g)
    Teardown           →  Udp.stop() / CoAPServer.stop()

See also
--------

* `Multicasting.md <https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md>`_ — Markdown copy in the library tree.
* :doc:`openthread_core` — ``subscribeMulticast`` / ``unsubscribeMulticast`` API.
* :doc:`openthread_udp` — ``OThreadUDP`` including ``beginMulticast``.
* :doc:`openthread_coap` — CoAP multicast and RFC notes.
* :doc:`openthread` — overview and shutdown order.
