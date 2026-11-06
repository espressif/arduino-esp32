################
OThreadUDP Class
################

About
-----

The ``OThreadUDP`` class is an Arduino ``UDP`` implementation backed
directly by the native OpenThread ``otUdpSocket`` API. It does **not**
go through lwIP, so it is the lightest path to send and receive IPv6
UDP datagrams over the Thread mesh and avoids duplicate IP stacks on
small SoCs (ESP32-H2, ESP32-C6, ESP32-C5).

For multicast receivers, senders, scopes, and examples see :doc:`Multicast guide <Multicasting>`.

**Key Features:**

* Full Arduino ``UDP`` interface (``begin``, ``beginMulticast``,
  ``beginPacket``, ``write``, ``endPacket``, ``parsePacket``, ``read``,
  ``peek``, ``flush``, ``remoteIP``, ``remotePort``).
* Pure raw ``otUdpSocket`` - no lwIP dependency.
* Uses a FreeRTOS queue to buffer incoming datagrams between the
  OpenThread receive callback and the sketch's ``loop()``.
* Drop-oldest policy when the RX queue is full, so ``parsePacket()``
  always returns fresh data instead of stalling on stale packets.
* Configurable maximum datagram size and queue depth via build-time
  defines.

**Use Cases:**

* IPv6 unicast or multicast UDP messaging between Thread devices.
* Low-overhead device-to-device telemetry over a Thread mesh.
* Anywhere a regular Arduino ``UDP`` sketch is needed but on top of
  Thread instead of Wi-Fi/Ethernet.

API Reference
-------------

The class inherits from Arduino's abstract ``UDP`` base class. All
arguments and return values follow the standard Arduino ``UDP``
semantics. The methods specific to OpenThread are highlighted.

Constructor
***********

.. code-block:: arduino

    OThreadUDP();
    ~OThreadUDP();

The default constructor does not open any socket - call ``begin()`` to
actually create the underlying ``otUdpSocket``. The destructor calls
``stop()`` automatically.

operator bool
^^^^^^^^^^^^^

.. code-block:: arduino

    operator bool() const;

Returns ``true`` when the underlying ``otUdpSocket`` is open (i.e.
between a successful ``begin()`` and the matching ``stop()``).

Socket Lifecycle
****************

begin (IPv6 address + port)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    uint8_t begin(IPAddress addr, uint16_t port);

* ``addr`` - Local IPv6 address to bind to. Use ``OT_IN6ADDR_ANY`` to
  accept on any local address.
* ``port`` - Local UDP port.

Opens the socket (``otUdpOpen``) and binds it
(``otUdpBind(..., OT_NETIF_THREAD_INTERNAL)``). Returns ``1`` on
success, ``0`` on failure.

begin (port only)
^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    uint8_t begin(uint16_t port);

Convenience overload that binds on ``OT_IN6ADDR_ANY``.

beginMulticast
^^^^^^^^^^^^^^

.. code-block:: arduino

    uint8_t beginMulticast(IPAddress group, uint16_t port);

* ``group`` - IPv6 multicast group to subscribe to (e.g. ``ff03::1``).
* ``port``  - Local UDP port.

Internally calls ``begin(port)`` first and then
``otIp6SubscribeMulticastAddress`` for ``group``. The subscription is
released symmetrically by ``stop()``. See :doc:`Multicast guide <Multicasting>`.

stop
^^^^

.. code-block:: arduino

    void stop();

Frees any pending TX message buffer, unsubscribes the multicast group
(if one was joined), and closes the ``otUdpSocket``. Safe to call
multiple times.

Sending Packets
***************

beginPacket (IPAddress)
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    int beginPacket(IPAddress ip, uint16_t port);

* ``ip``   - Destination IPv6 address.
* ``port`` - Destination UDP port.

Allocates an ``otMessage`` via ``otUdpNewMessage``, stores the peer
address/port, and prepares the socket for ``write()`` calls. Any
previously-pending TX buffer is freed first.

Returns ``1`` on success, ``0`` if no message buffer was available.

beginPacket (textual host)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    int beginPacket(const char *host, uint16_t port);

Convenience overload that parses ``host`` as an IPv6 address using
``IPAddress::fromString`` and forwards to the
``beginPacket(IPAddress, port)`` overload. Returns ``0`` if the
address is not a valid IPv6 literal.

write
^^^^^

.. code-block:: arduino

    size_t write(uint8_t b);
    size_t write(const uint8_t *buf, size_t size);

Appends bytes to the pending TX message using ``otMessageAppend``.
Returns the number of bytes actually queued (``0`` on failure or if
no ``beginPacket`` is pending).

endPacket
^^^^^^^^^

.. code-block:: arduino

    int endPacket();

Submits the TX message via ``otUdpSend``. On success OpenThread takes
ownership of the message buffer; on failure ``OThreadUDP`` frees it
itself so no buffer leaks. Returns ``1`` on success, ``0`` on failure.

Receiving Packets
*****************

The internal RX callback runs in the OpenThread task with the API
lock held. It snapshots the payload plus ``mPeerAddr`` / ``mPeerPort``
into a per-instance FreeRTOS queue. ``parsePacket()`` then pops one
datagram into a per-instance current-packet buffer that ``read()``,
``available()``, ``peek()`` and ``flush()`` walk through.

If the queue is full when a new datagram arrives, the **oldest** entry
is dropped to make room. This keeps the queue from becoming a buffer
of stale data when the sketch falls behind.

parsePacket
^^^^^^^^^^^

.. code-block:: arduino

    int parsePacket();

Pops the next received datagram (if any) into the current-packet
buffer and returns its length. Returns ``0`` when the queue is empty.
Successive calls drop any unread bytes from the previous packet.

available
^^^^^^^^^

.. code-block:: arduino

    int available();

Returns the number of unread bytes left in the current packet.

read
^^^^

.. code-block:: arduino

    int read();
    int read(unsigned char *buf, size_t len);
    int read(char *buf, size_t len);

Reads from the current packet. The byte form returns ``-1`` if no data
is left; the buffer forms return the number of bytes copied (``0`` if
no packet is current).

peek
^^^^

.. code-block:: arduino

    int peek();

Returns the next byte in the current packet without consuming it
(``-1`` if no byte is available).

flush
^^^^^

.. code-block:: arduino

    void flush();

Discards any remaining bytes of the current RX packet.

remoteIP
^^^^^^^^

.. code-block:: arduino

    IPAddress remoteIP();

Returns the sender's IPv6 address of the most recently parsed packet
(``IPAddress(IPv6)`` if no packet has been parsed yet).

remotePort
^^^^^^^^^^

.. code-block:: arduino

    uint16_t remotePort();

Returns the sender's UDP port of the most recently parsed packet
(``0`` if no packet has been parsed yet).

Constants
*********

OT_IN6ADDR_ANY
^^^^^^^^^^^^^^

.. code-block:: arduino

    extern const IPAddress OT_IN6ADDR_ANY;

The IPv6 "any" address (``::``), suitable for binding to all local
IPv6 interfaces on a port. Equivalent to ``IPAddress(IPv6)`` but more
self-documenting at call sites.

Build-time tunables
*******************

The following defines control the RX path; override them on the
compiler command line if your traffic profile needs more headroom.

OT_UDP_MAX_PACKET_SIZE
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #define OT_UDP_MAX_PACKET_SIZE 512    // default

Maximum payload bytes of a single received datagram that the RX queue
will store. Larger packets are **truncated** to this size on arrival.

OT_UDP_RX_QUEUE_DEPTH
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #define OT_UDP_RX_QUEUE_DEPTH 4       // default

Number of datagrams that may sit in the FreeRTOS RX queue between
calls to ``parsePacket()``. When the queue is full the **oldest**
entry is dropped to make room for the new one.

Memory note: each ``OThreadUDP`` instance allocates one RX queue of
``OT_UDP_RX_QUEUE_DEPTH * (OT_UDP_MAX_PACKET_SIZE + ~20)`` bytes once
``begin()`` succeeds. The default settings cost ~2 KB per instance.

Example
-------

Basic UDP Echo
**************

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadUDP.h>

    // ff02::1 = link-local all-nodes multicast
    const uint8_t serverBytes[16] = {0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    IPAddress server(IPv6, serverBytes);

    OThreadUDP otUdp;

    void setup() {
        Serial.begin(115200);

        // Bring up Thread (using a DataSet, or via the Joiner role).
        OThread.begin(false);
        // ... configure dataset and call commitDataSet(), then
        // networkInterfaceUp() + start(); or, for a Joiner device,
        // networkInterfaceUp() + startJoiner() + start() on success.
        OThread.networkInterfaceUp();
        OThread.start();

        // Bind a UDP socket on port 7, any IPv6 local address.
        otUdp.begin(7);
    }

    void loop() {
        // Send "Hello" to the link-local all-nodes group, port 7.
        otUdp.beginPacket(server, 7);
        otUdp.write((const uint8_t *)"Hello", 5);
        otUdp.endPacket();

        // Drain any pending datagrams.
        while (int n = otUdp.parsePacket()) {
            char buf[64];
            int  r = otUdp.read(buf, sizeof(buf) - 1);
            buf[r] = 0;
            Serial.printf("From [%s]:%u -> '%s'\r\n",
                          otUdp.remoteIP().toString().c_str(),
                          otUdp.remotePort(), buf);
        }

        delay(1000);
    }

Multicast Receiver
******************

.. code-block:: arduino

    OThreadUDP otUdp;
    const uint8_t realmLocalBytes[16] = {0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    IPAddress realmLocalAllNodes(IPv6, realmLocalBytes);  // ff03::1

    void setup() {
        // ... bring up Thread ...
        otUdp.beginMulticast(realmLocalAllNodes, 7);
    }

    void loop() {
        if (int n = otUdp.parsePacket()) {
            char buf[64];
            int  r = otUdp.read(buf, sizeof(buf) - 1);
            buf[r] = 0;
            Serial.printf("Multicast from [%s]:%u -> '%s'\r\n",
                          otUdp.remoteIP().toString().c_str(),
                          otUdp.remotePort(), buf);
        }
    }

Best Practices
--------------

* **Open after attach**: only call ``otUdp.begin()`` after the device is
  attached to the network (role is Child / Router / Leader). Binding
  before attach succeeds, but no traffic will flow until the IPv6
  stack acquires its mesh-local address.
* **Avoid blocking the OT task**: the RX callback runs in the
  OpenThread task with the API lock held - it must finish quickly.
  Do not call back into ``otXxx(...)`` functions from a sketch
  ``loop()`` while another task holds the lock.
* **Tune queue depth**: if your application can fall behind (e.g.
  expensive ``Serial.print`` per packet) and you cannot afford to drop
  datagrams, increase ``OT_UDP_RX_QUEUE_DEPTH`` at the cost of RAM.
* **MTU**: Thread message buffers are small. Keep individual
  datagrams below ~1280 bytes (the IPv6 minimum link MTU) and
  preferably much smaller for reliability.
* **Multicast scope**: ``ff02::`` is link-local (single Thread hop),
  ``ff03::`` is realm-local (entire Thread partition). Choose
  according to how far the message must propagate. Details: :doc:`Multicast guide <Multicasting>`.
* **Application ports**: avoid OpenThread-reserved ports for application
  sockets. ``5683`` / ``5684`` are CoAP/CoAPs ports and ``61631`` is
  Thread TMF CoAP. The Native UDP examples use ``5050`` and ``5051``.

Shutdown
--------

``OpenThread::end()`` does **not** close application UDP sockets for you.
If your sketch called ``OThreadUDP.begin()`` or ``beginMulticast()``, call
``OThreadUDP.stop()`` **before** ``OThread.end()`` so the underlying
``otUdp`` socket and RX queue are released in the documented order (CoAP
stop/destroy first when CoAP is used, then UDP, then stack deinit). See
:doc:`openthread` (Shutdown Order) and the Native
`StackShutdown example <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown>`_.
