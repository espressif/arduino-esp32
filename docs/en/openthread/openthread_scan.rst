##################
OThreadScan Class
##################

About
-----

The ``OThreadScan`` class discovers nearby Thread networks using MLE discover
(``otThreadDiscover()`` / OpenThread CLI ``discover``). Each result is an
``OThreadNetworkInfo`` structure with Thread identity fields (network name,
Extended PAN ID, joinable flag) and IEEE 802.15.4 link fields (extended
address, PAN ID, channel, RSSI, LQI).

This is the same primitive Matter uses to list Thread networks during
commissioning. The global instance is ``OThreadScan`` (same pattern as
``OThreadUDP`` and ``OThreadCoAPServer``).

**Key Features:**

* Single entry point: ``discoverNetworks()`` — no separate 802.15.4 or energy
  scan modes in the public API.
* Blocking, async poll (``scanComplete()``), and streaming callbacks
  (``onResult()`` / ``onComplete()``).
* Optional discover filters (PAN ID, joiner-only, EUI-64) matching
  ``otThreadDiscover()`` parameters.
* WiFi-scan-style return codes: ``OT_DISCOVER_RUNNING`` (-1),
  ``OT_DISCOVER_FAILED`` (-2).

**Use Cases:**

* List Thread networks before joining or commissioning.
* Build a network-picker UI on a joiner device.
* Debug RF environment when a Leader is expected nearby.

**Prerequisites:**

* ``OThread.begin()`` and ``OThread.networkInterfaceUp()`` — Thread does **not**
  need to be started (matches Matter pre-commission discovery).
* For meaningful results, at least one Leader or Router should be active
  nearby.

Include the header:

.. code-block:: arduino

    #include <OThreadScan.h>

OThreadNetworkInfo
------------------

Each discovery response is stored as ``OThreadNetworkInfo``:

+----------------------+--------------------------------------------------+
| Field                | Description                                      |
+======================+==================================================+
| ``networkName``      | Thread network name (null-terminated)            |
+----------------------+--------------------------------------------------+
| ``extendedPanId``    | Extended PAN ID (8 bytes)                        |
+----------------------+--------------------------------------------------+
| ``panId``            | IEEE 802.15.4 PAN ID                             |
+----------------------+--------------------------------------------------+
| ``extAddress``       | Responder extended address (8 bytes)             |
+----------------------+--------------------------------------------------+
| ``channel``          | IEEE 802.15.4 channel (11..26)                   |
+----------------------+--------------------------------------------------+
| ``rssi``             | Received signal strength (dBm)                   |
+----------------------+--------------------------------------------------+
| ``lqi``              | Link Quality Indicator                           |
+----------------------+--------------------------------------------------+
| ``threadVersion``    | Thread version (4-bit MLE value)                 |
+----------------------+--------------------------------------------------+
| ``joinable``         | Join-permitted flag when known                   |
+----------------------+--------------------------------------------------+
| ``nativeCommissioner`` | Native Commissioner flag when present          |
+----------------------+--------------------------------------------------+

Helper methods: ``networkNameStr()``, ``extendedPanIdStr()``,
``extAddressStr()``.

OThreadDiscoverFilters
----------------------

Optional filters for ``setDiscoverFilters()`` (defaults match CLI
``discover`` with no extra arguments):

.. code-block:: arduino

    OThreadDiscoverFilters filters;
    filters.panIdFilter = OT_PANID_BROADCAST;  // 0xffff = no PAN filter
    filters.joinerOnly = false;
    filters.eui64Filter = false;
    OThreadScan.setDiscoverFilters(filters);

API Reference
-------------

Configuration
*************

setScanTimeout
^^^^^^^^^^^^^^

.. code-block:: arduino

    void setScanTimeout(uint32_t ms);

Maximum time to wait for blocking discovery or async completion. Default:
``OT_DISCOVER_DEFAULT_TIMEOUT_MS`` (30000 ms).

setChannel
^^^^^^^^^^

.. code-block:: arduino

    void setChannel(uint8_t channel);

Limit discovery to one IEEE 802.15.4 channel (11..26). Pass ``0`` to scan all
supported channels (channel mask ``0`` in ``otThreadDiscover()``).

setDiscoverFilters
^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    void setDiscoverFilters(const OThreadDiscoverFilters &filters);

Apply PAN / joiner / EUI-64 filters before the next ``discoverNetworks()``
call.

Callbacks
*********

onResult
^^^^^^^^

.. code-block:: arduino

    void onResult(OThreadDiscoverResultCallback callback, void *context = nullptr);

Called for each network as OpenThread receives a Discovery Response. Matches
the OpenThread / Matter streaming model.

onComplete
^^^^^^^^^^

.. code-block:: arduino

    void onComplete(OThreadDiscoverCompleteCallback callback, void *context = nullptr);

Called when discovery finishes. ``resultCount`` is the number of networks
collected, or ``OT_DISCOVER_FAILED`` on failure. ``error`` is the OpenThread
error from the start request (``OT_ERROR_NONE`` on success).

Discovery
*********

discoverNetworks
^^^^^^^^^^^^^^^^

.. code-block:: arduino

    int16_t discoverNetworks(bool async = false);

Start MLE Thread network discovery.

* ``async == false`` — blocks until complete or ``setScanTimeout()`` expires;
  returns network count on success.
* ``async == true`` — returns ``OT_DISCOVER_RUNNING`` immediately on success.

Return values:

* ``>= 0`` — number of networks found (blocking mode, or ``scanComplete()`` when
  done).
* ``OT_DISCOVER_RUNNING`` (-1) — discovery in progress.
* ``OT_DISCOVER_FAILED`` (-2) — failed, timed out, or not started.

scanComplete
^^^^^^^^^^^^

.. code-block:: arduino

    int16_t scanComplete();

Poll async discovery state from ``loop()``. Same return semantics as
``discoverNetworks()`` in blocking/async modes.

scanDelete
^^^^^^^^^^

.. code-block:: arduino

    void scanDelete();

Release stored results and free associated memory. Call after processing
results.

isDiscoverInProgress
^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool isDiscoverInProgress() const;

Returns ``true`` while an MLE discovery scan is in progress.

Results
*******

getResultCount
^^^^^^^^^^^^^^

.. code-block:: arduino

    uint16_t getResultCount() const;

Number of entries from the last completed discovery (saturated at
``UINT16_MAX`` if the internal list is larger).

getResult
^^^^^^^^^

.. code-block:: arduino

    const OThreadNetworkInfo &getResult(uint8_t index) const;
    bool getResult(uint8_t index, OThreadNetworkInfo &info) const;

Access one result by index (0 .. count-1). Internal storage is valid until
``scanDelete()``.

Index-based convenience getters (``networkName()``, ``panId()``,
``extendedPanIdStr()``, ``extAddressStr()``, ``channel()``, ``rssi()``,
``lqi()``, ``isJoinable()``, ``threadVersion()``, ``isNativeCommissioner()``)
delegate to ``getResult(index)``.

getActiveScanResult
^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    const otActiveScanResult *getActiveScanResult(uint8_t index) const;

Raw OpenThread result for advanced use. Valid until ``scanDelete()``.

Usage Patterns
--------------

Blocking (WiFiScan-style)
*************************

.. code-block:: arduino

    #include <OThread.h>
    #include <OThreadScan.h>

    void setup() {
        Serial.begin(115200);
        OThread.begin(false);
        OThread.networkInterfaceUp();

        int n = OThreadScan.discoverNetworks();
        if (n >= 0) {
            for (int i = 0; i < n; ++i) {
                const OThreadNetworkInfo &net = OThreadScan.getResult(i);
                Serial.println(net.networkNameStr());
            }
            OThreadScan.scanDelete();
        }
    }

Async poll (WiFiScanAsync-style)
********************************

.. code-block:: arduino

    OThreadScan.discoverNetworks(true);

    void loop() {
        int16_t status = OThreadScan.scanComplete();
        if (status >= 0) {
            // process OThreadScan.getResult(i) ...
            OThreadScan.scanDelete();
        }
    }

Streaming callbacks
*****************

.. code-block:: arduino

    static void onNetwork(const OThreadNetworkInfo &info, void *) {
        Serial.println(info.networkNameStr());
    }

    OThreadScan.onResult(onNetwork);
    OThreadScan.discoverNetworks(true);

Examples
--------

Native API examples (``libraries/OpenThread/examples/Native/ThreadScan/``):

* `ThreadScan_Discover <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Discover>`_ — blocking discovery.
* `ThreadScan_Async <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Async>`_ — ``scanComplete()`` polling.
* `ThreadScan_Callback <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/ThreadScan/ThreadScan_Callback>`_ — ``onResult()`` / ``onComplete()``.

CLI equivalent: `CLI ThreadScan <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/ThreadScan>`_ (`discover` command via ``OThreadCLI``).

Troubleshooting
---------------

**No networks found**
  * Start a Leader or Router on another board first (for example
    `SimpleThreadNetwork LeaderNode <https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/SimpleThreadNetwork/LeaderNode>`_).
  * Confirm ``OThread.networkInterfaceUp()`` was called before
    ``discoverNetworks()``.

**``OT_DISCOVER_FAILED``**
  * Discovery already in progress, IPv6 interface down, or timeout — increase
    ``setScanTimeout()`` or call ``scanDelete()`` before restarting.

**Wrong network listed**
  * Multiple partitions nearby — compare ``extendedPanId`` with the target
    Leader.

See also
--------

* :doc:`openthread` — overview and Native vs CLI comparison.
* :doc:`openthread_core` — ``OThread.begin()``, ``networkInterfaceUp()``.
* :doc:`openthread_cli` — CLI ``discover`` command.
