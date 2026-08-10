################
OpenThread Class
################

About
-----

The ``OpenThread`` class provides direct access to OpenThread API functions for managing Thread network operations. This is the **Classes API** approach, which offers object-oriented methods that directly call OpenThread API functions.

**Key Features:**
* Direct OpenThread API access.
* Network management (start, stop, interface control).
* Dataset management.
* Live setters that act directly on the running instance (no DataSet required).
* Thread Joiner role (synchronous commissioning of a new device using a PSKd).
* Thread Commissioner role (let new devices attach with just a PSKd).
* Address management with caching.
* Network information retrieval.
* Device role monitoring.

**Use Cases:**
* Thread network configuration and management.
* Direct control over Thread operations.
* Programmatic network setup.
* Commissioning new devices over-the-air with a PSKd.
* Address and routing information access.

API Reference
-------------

Initialization
**************

begin
^^^^^

Initializes the OpenThread stack.

.. code-block:: arduino

    static void begin(bool OThreadAutoStart = true);

* ``OThreadAutoStart`` - If ``true``, automatically starts Thread with default dataset from NVS or ESP-IDF settings (default: ``true``).

This function initializes the OpenThread stack and creates the OpenThread task. If ``OThreadAutoStart`` is ``true``, it will attempt to start Thread using the active dataset from NVS or ESP-IDF default settings.

``begin()`` returns only after the worker task has finished initializing the OpenThread stack and reloaded any persisted Active Operational Dataset from NVS. After it returns successfully, APIs such as ``hasActiveDataset()`` can reliably query the stack state without an additional delay loop.

**Note:** This is a static function and should be called before creating an ``OpenThread`` instance.

end
^^^

Stops and cleans up the OpenThread stack.

.. code-block:: arduino

    static void end();

This function stops the OpenThread task and cleans up all OpenThread resources. It should be called when you no longer need the OpenThread stack.

**Note:** This is a static function.

Network Control
***************

start
^^^^^

Starts the Thread network.

.. code-block:: arduino

    void start();

This function enables the Thread network. The device will attempt to join or form a Thread network based on the active dataset.

**Note:** The network interface must be brought up separately using ``networkInterfaceUp()``.

stop
^^^^

Stops the Thread network.

.. code-block:: arduino

    void stop();

This function disables the Thread network. The device will leave the Thread network and stop participating in Thread operations.

networkInterfaceUp
^^^^^^^^^^^^^^^^^^

Brings the Thread network interface up.

.. code-block:: arduino

    void networkInterfaceUp();

This function enables the Thread IPv6 interface (equivalent to CLI command ``ifconfig up``). The device will be able to send and receive IPv6 packets over the Thread network.

networkInterfaceDown
^^^^^^^^^^^^^^^^^^^^

Brings the Thread network interface down.

.. code-block:: arduino

    void networkInterfaceDown();

This function disables the Thread IPv6 interface (equivalent to CLI command ``ifconfig down``). The device will stop sending and receiving IPv6 packets. Clears the cached unicast/multicast address lists (same as ``networkInterfaceUp()`` and ``start()`` / ``stop()``).

Dataset Management
******************

commitDataSet
^^^^^^^^^^^^^

Commits an operational dataset to the Thread network.

.. code-block:: arduino

    void commitDataSet(const DataSet &dataset);

* ``dataset`` - The ``DataSet`` object containing the operational dataset parameters.

This function sets the active operational dataset for the Thread network. The dataset must be properly configured before committing.

**Example:**

.. code-block:: arduino

    DataSet dataset;
    dataset.initNew();
    dataset.setNetworkName("MyThreadNetwork");
    dataset.setChannel(15);
    dataset.setNetworkKey(networkKey);
    OThread.commitDataSet(dataset);

hasActiveDataset
^^^^^^^^^^^^^^^^

Checks whether OpenThread has a committed Active Operational Dataset.

.. code-block:: arduino

    bool hasActiveDataset() const;

This function returns ``true`` when an Active Operational Dataset is available in the OpenThread stack. The dataset may have been loaded from NVS during ``OpenThread::begin()`` or committed by the application with ``commitDataSet()``.

This is useful when a sketch needs to decide whether to resume an existing Thread network or provision a new one:

.. code-block:: arduino

    OpenThread::begin(false);

    if (OThread.hasActiveDataset()) {
        OThread.networkInterfaceUp();
        OThread.start();
    } else {
        DataSet dataset;
        dataset.initNew();
        dataset.setNetworkName("MyThreadNetwork");
        dataset.setChannel(15);
        OThread.commitDataSet(dataset);
        OThread.networkInterfaceUp();
        OThread.start();
    }

getCurrentDataSet
^^^^^^^^^^^^^^^^^

Gets the current active operational dataset.

.. code-block:: arduino

    const DataSet &getCurrentDataSet() const;

This function returns a reference to a ``DataSet`` object containing the current active operational dataset parameters.

Network Information
*******************

getNetworkName
^^^^^^^^^^^^^^

Gets the Thread network name.

.. code-block:: arduino

    String getNetworkName() const;

This function returns the network name as a ``String``.

getExtendedPanId
^^^^^^^^^^^^^^^^

Gets the extended PAN ID.

.. code-block:: arduino

    const uint8_t *getExtendedPanId() const;

This function returns a pointer to an 8-byte array containing the extended PAN ID.

getNetworkKey
^^^^^^^^^^^^^

Gets the network key.

.. code-block:: arduino

    const uint8_t *getNetworkKey() const;

This function returns a pointer to a 16-byte array containing the network key.

**Note:** The network key is stored in static storage and persists after the function returns.

getChannel
^^^^^^^^^^

Gets the Thread channel.

.. code-block:: arduino

    uint8_t getChannel() const;

This function returns the Thread channel number (11-26).

getPanId
^^^^^^^^

Gets the PAN ID.

.. code-block:: arduino

    uint16_t getPanId() const;

This function returns the PAN ID as a 16-bit value.

Live Setters
************

These setters apply changes directly to the running OpenThread instance.
They are an alternative to building an offline ``DataSet`` and calling
``commitDataSet()`` - useful when commissioning a new device via the
Joiner role, where the dataset is supplied by the commissioner and only a
few parameters (channel, PAN ID, extended PAN ID) need to be pre-set on
the joiner side.

All setters return an ``otError`` value (``OT_ERROR_NONE`` on success,
``OT_ERROR_INVALID_STATE`` if OpenThread is not initialized,
``OT_ERROR_INVALID_ARGS`` for a null argument, ``OT_ERROR_FAILED`` if the
OpenThread API lock could not be acquired). They acquire the OpenThread
API lock internally and are safe to call from any FreeRTOS task. The
Thread protocol should normally be stopped (``stop()``) before changing
these parameters on an already-attached device.

setChannel
^^^^^^^^^^

Sets the IEEE 802.15.4 channel of the running instance.

.. code-block:: arduino

    otError setChannel(uint8_t channel);

* ``channel`` - Channel number (11..26).

Wraps ``otLinkSetChannel``.

setPanId
^^^^^^^^

Sets the PAN ID of the running instance.

.. code-block:: arduino

    otError setPanId(uint16_t panid);

Wraps ``otLinkSetPanId``.

setExtendedPanId
^^^^^^^^^^^^^^^^

Sets the 8-byte Extended PAN ID of the running instance.

.. code-block:: arduino

    otError setExtendedPanId(const uint8_t *extpanid);

* ``extpanid`` - Pointer to an 8-byte array (``OT_EXT_PAN_ID_SIZE``).

Wraps ``otThreadSetExtendedPanId``.

setNetworkKey
^^^^^^^^^^^^^

Sets the 16-byte Network Key of the running instance.

.. code-block:: arduino

    otError setNetworkKey(const uint8_t *key);

* ``key`` - Pointer to a 16-byte array (``OT_NETWORK_KEY_SIZE``).

Wraps ``otThreadSetNetworkKey``.

setNetworkName
^^^^^^^^^^^^^^

Sets the network name of the running instance.

.. code-block:: arduino

    otError setNetworkName(const char *name);

* ``name`` - Null-terminated string, up to 16 ASCII characters.

Wraps ``otThreadSetNetworkName``.

**Example - Joiner side, pre-configure channel/PAN before joining:**

.. code-block:: arduino

    OThread.begin(false);                       // stack up, no DataSet
    OThread.setChannel(15);                     // pre-pick channel
    OThread.setPanId(0xFFFF);                   // pre-pick PAN
    uint8_t xp[8] = {0x11,0x11,0x11,0x11,0x22,0x22,0x22,0x22};
    OThread.setExtendedPanId(xp);
    OThread.networkInterfaceUp();

    otError err = OThread.startJoiner("J01NME"); // run Joiner state machine
    if (err == OT_ERROR_NONE) {
        OThread.start();                         // enable Thread with provisioned dataset
    }

Device Identity
***************

These read-only getters expose identity / version / radio parameters of
the running instance.

getEui64
^^^^^^^^

Returns the factory-assigned IEEE EUI-64 (the device's hardware extended
address).

.. code-block:: arduino

    bool   getEui64(uint8_t out[8]) const;
    String getEui64() const;

* The byte-array form copies 8 bytes into ``out`` and returns ``true`` on
  success.
* The ``String`` form returns the EUI-64 as a 16-character lowercase hex
  string (empty ``String`` on failure).

Wraps ``otLinkGetFactoryAssignedIeeeEui64``.

getExtendedAddress
^^^^^^^^^^^^^^^^^^

Returns the **current** IEEE 802.15.4 extended address used by the radio.
This may differ from the factory EUI-64 (for example during
commissioning the Joiner ID is used in its place).

.. code-block:: arduino

    const uint8_t *getExtendedAddress() const;

The returned pointer is stack-owned and valid only transiently. Wraps
``otLinkGetExtendedAddress``.

getThreadVersion
^^^^^^^^^^^^^^^^

Returns the Thread protocol version implemented by the stack (for example ``4`` for Thread 1.3).

.. code-block:: arduino

    uint16_t getThreadVersion() const;

Wraps ``otThreadGetVersion``.

getVersionString
^^^^^^^^^^^^^^^^

Returns a human-readable OpenThread stack version string
(e.g. ``"OPENTHREAD/thread-reference-...; ..."``).

.. code-block:: arduino

    String getVersionString() const;

Wraps ``otGetVersionString``.

getTxPower
^^^^^^^^^^

Returns the configured radio transmit power, in dBm, or ``INT8_MIN`` on
failure.

.. code-block:: arduino

    int8_t getTxPower() const;

Wraps ``otPlatRadioGetTransmitPower``.

getPollPeriod
^^^^^^^^^^^^^

Returns the Sleepy End Device data poll period, in milliseconds.

.. code-block:: arduino

    uint32_t getPollPeriod() const;

Wraps ``otLinkGetPollPeriod``.

Joiner Role
***********

The Joiner role is used by a brand-new device to attach to a Thread
network without knowing the network key. It only needs a PSKd
(Pre-Shared Key for Device) shared with the network's Commissioner.

The OpenThread Arduino wrapper exposes a **synchronous** ``startJoiner``
helper: it runs the OpenThread Joiner state machine in the background OT
task and blocks the calling task on a FreeRTOS semaphore until the
commissioning callback fires (or the timeout elapses).

**Order of calls:**

1. ``OThread.begin(false);``  - start the stack without auto-loading a
   DataSet.
2. (optional) ``setChannel`` / ``setPanId`` / ``setExtendedPanId`` so the
   joiner does not have to scan every channel.
3. ``OThread.networkInterfaceUp();``  - IPv6 stack must be up before
   ``startJoiner``.
4. ``OThread.startJoiner(PSKD);``     - blocks until success / failure.
   Thread must **not** be enabled yet (``start()`` must not have been
   called).
5. On success, ``OThread.start();``   - enable Thread protocol with the
   dataset just provisioned by the commissioner.

**Required Kconfig:** ``CONFIG_OPENTHREAD_JOINER=y``.

startJoiner
^^^^^^^^^^^

Starts the Joiner role and synchronously waits for completion.

.. code-block:: arduino

    otError startJoiner(const char *pskd,
                        uint32_t    timeoutMs       = 30000,
                        const char *provisioningUrl = nullptr,
                        const char *vendorName      = nullptr,
                        const char *vendorModel     = nullptr,
                        const char *vendorSwVersion = nullptr,
                        const char *vendorData      = nullptr);

* ``pskd`` - Pre-Shared Key for Device. ASCII, 6..32 characters, must
  match the value the commissioner accepted via ``addJoiner()``. Must not
  contain the characters ``I``, ``O``, ``Q`` or the digit ``0``
  (base32-thread alphabet).
* ``provisioningUrl`` / ``vendorName`` / ``vendorModel`` /
  ``vendorSwVersion`` / ``vendorData`` - Optional MeshCoP TLVs passed to
  the commissioner during the Joiner Finalize message. May be ``nullptr``.
* ``timeoutMs`` - Maximum time the wrapper waits on its semaphore for the
  ``otJoinerCallback`` to fire. The OpenThread stack also has its own
  internal Joiner timers. Default is 30 s.

**Returns:**

* ``OT_ERROR_NONE``               - successfully commissioned.
* ``OT_ERROR_SECURITY``           - PSKd mismatch.
* ``OT_ERROR_NOT_FOUND``          - no joinable network was discovered.
* ``OT_ERROR_RESPONSE_TIMEOUT``   - the commissioner stopped responding
  or the wrapper timeout expired (the wrapper also calls ``otJoinerStop``
  in this case).
* ``OT_ERROR_INVALID_STATE``      - IPv6 stack not up, or Thread already
  enabled.

Wraps ``otJoinerStart``.

stopJoiner
^^^^^^^^^^

Aborts an in-progress Joiner state machine.

.. code-block:: arduino

    void stopJoiner();

Wraps ``otJoinerStop``.

getJoinerState
^^^^^^^^^^^^^^

Returns the live Joiner state (``OT_JOINER_STATE_IDLE`` /
``DISCOVER`` / ``CONNECT`` / ``CONNECTED`` / ``ENTRUST`` / ``JOINED``).

.. code-block:: arduino

    otJoinerState getJoinerState() const;

Wraps ``otJoinerGetState``.

getJoinerId
^^^^^^^^^^^

Returns the device's Joiner ID. By default this is the first 64 bits of
``SHA-256(EUI-64)`` and is also used as the 802.15.4 extended address
during commissioning.

.. code-block:: arduino

    const otExtAddress *getJoinerId() const;

Wraps ``otJoinerGetId``.

**Example - Joiner:**

.. code-block:: arduino

    OThread.begin(false);
    OThread.setChannel(15);
    OThread.networkInterfaceUp();

    otError err = OThread.startJoiner("J01NME");
    if (err == OT_ERROR_NONE) {
        OThread.start();                 // enable Thread with provisioned dataset
    } else {
        Serial.printf("Join failed: %d\r\n", err);
    }

Commissioner Role
*****************

The Commissioner is the network-side counterpart of the Joiner: it
authorizes new devices to attach using a PSKd, and securely hands the
operational dataset to them. A Commissioner must already be **attached**
to a Thread network (typically as the Leader).

The Arduino wrapper exposes a **synchronous** ``startCommissioner``: it
petitions the Commissioner role and blocks until either
``OT_COMMISSIONER_STATE_ACTIVE`` is reached or the petition is rejected.

**Order of calls:**

1. Form or resume a network (``commitDataSet()`` or NVS resume).
2. ``OThread.networkInterfaceUp();`` + ``OThread.start();`` - bring up
   Thread and wait until the role is no longer Detached / Disabled.
3. ``OThread.startCommissioner();`` - blocks until the petition succeeds.
4. ``OThread.addJoiner(PSKD);`` - open the joiner window for incoming
   devices.

**Required Kconfig:** ``CONFIG_OPENTHREAD_COMMISSIONER=y``.

startCommissioner
^^^^^^^^^^^^^^^^^

Petitions for the active Commissioner role and waits for the petition to
resolve.

.. code-block:: arduino

    otError startCommissioner(uint32_t timeoutMs = 30000);

* ``timeoutMs`` - How long to wait for the petition to be granted before
  giving up. Default 30 s.

**Returns:**

* ``OT_ERROR_NONE``             - now in ``OT_COMMISSIONER_STATE_ACTIVE``.
* ``OT_ERROR_REJECTED``         - petition rejected (another commissioner
  already active in the partition, or local commissioner state fell back
  to ``DISABLED``).
* ``OT_ERROR_RESPONSE_TIMEOUT`` - petition did not resolve within
  ``timeoutMs``. The wrapper calls ``otCommissionerStop`` to clean up.
* ``OT_ERROR_INVALID_STATE``    - device not attached to a network.

Wraps ``otCommissionerStart``.

addJoiner
^^^^^^^^^

Authorizes a remote joiner that presents the given PSKd.

.. code-block:: arduino

    otError addJoiner(const char *pskd,
                      uint32_t timeoutSec       = 120,
                      const otExtAddress *eui64 = nullptr);

* ``pskd`` - PSKd that the joiner must present (must match the joiner
  side's ``startJoiner`` argument).
* ``eui64`` - EUI-64 of the specific joiner that is allowed, or
  ``nullptr`` to accept any joiner (equivalent to ``*`` in ``ot-ctl``).
* ``timeoutSec`` - How long the joiner entry stays valid on the
  commissioner (default 120 s). Once expired, the joiner must be added
  again.

**Returns:**

* ``OT_ERROR_NONE``             - joiner authorized.
* ``OT_ERROR_NO_BUFS``          - commissioner table is full.
* ``OT_ERROR_INVALID_ARGS``     - invalid EUI-64 or PSKd.
* ``OT_ERROR_INVALID_STATE``    - commissioner is not active.

Wraps ``otCommissionerAddJoiner``.

stopCommissioner
^^^^^^^^^^^^^^^^

Disables the Commissioner role.

.. code-block:: arduino

    void stopCommissioner();

Wraps ``otCommissionerStop``.

getCommissionerState
^^^^^^^^^^^^^^^^^^^^

Returns the live Commissioner state
(``OT_COMMISSIONER_STATE_DISABLED`` / ``PETITION`` / ``ACTIVE``).

.. code-block:: arduino

    otCommissionerState getCommissionerState() const;

Wraps ``otCommissionerGetState``.

**Example - Commissioner (run on an attached Leader/Router):**

.. code-block:: arduino

    // Already attached to the network at this point
    // (networkInterfaceUp() + start() completed earlier).
    otError err = OThread.startCommissioner();
    if (err == OT_ERROR_NONE) {
        OThread.addJoiner("J01NME",      // PSKd
                          120);          // window: 120 s (accepts any joiner)
    }

Timeouts at a glance
^^^^^^^^^^^^^^^^^^^^

Three independent timeouts are at play during a join:

1. **Wrapper join timeout** (``startJoiner(..., timeoutMs)``) - how long
   the Arduino wrapper blocks on its FreeRTOS semaphore waiting for the
   ``otJoinerCallback``. Default 30 s. On expiry the wrapper calls
   ``otJoinerStop`` and returns ``OT_ERROR_RESPONSE_TIMEOUT``.
2. **OpenThread stack timers** - internal MeshCoP / DTLS / channel-scan
   timers inside the Joiner state machine. NOT configurable from Arduino.
3. **Commissioner joiner window**
   (``addJoiner(..., timeoutSec)``) - how long the commissioner keeps
   the joiner entry alive. After this lifetime the entry is removed and
   the joiner must be re-added.

For interoperability with slow commissioners, set
``startJoiner(..., timeoutMs)`` slightly larger than
``addJoiner(..., timeoutSec) * 1000``.

Device Role
***********

otGetDeviceRole
^^^^^^^^^^^^^^^

Gets the current device role.

.. code-block:: arduino

    static ot_device_role_t otGetDeviceRole();

This function returns the current Thread device role:

* ``OT_ROLE_DISABLED`` - The Thread stack is disabled.
* ``OT_ROLE_DETACHED`` - Not currently participating in a Thread network.
* ``OT_ROLE_CHILD`` - The Thread Child role.
* ``OT_ROLE_ROUTER`` - The Thread Router role.
* ``OT_ROLE_LEADER`` - The Thread Leader role.

**Note:** This is a static function.

otGetStringDeviceRole
^^^^^^^^^^^^^^^^^^^^^

Gets the current device role as a string.

.. code-block:: arduino

    static const char *otGetStringDeviceRole();

This function returns a human-readable string representation of the current device role.

**Note:** This is a static function.

otPrintNetworkInformation
^^^^^^^^^^^^^^^^^^^^^^^^^

Prints network information to a Stream.

.. code-block:: arduino

    static void otPrintNetworkInformation(Stream &output);

* ``output`` - The Stream object to print to (e.g., ``Serial``).

This function prints comprehensive network information including:
* Device role.
* RLOC16.
* Network name.
* Channel.
* PAN ID.
* Extended PAN ID.
* Network key.

**Note:** This is a static function.

Address Management
******************

getMeshLocalPrefix
^^^^^^^^^^^^^^^^^^

Gets the mesh-local prefix.

.. code-block:: arduino

    const otMeshLocalPrefix *getMeshLocalPrefix() const;

This function returns a pointer to the mesh-local prefix structure.

getMeshLocalEid
^^^^^^^^^^^^^^^

Gets the mesh-local EID (Endpoint Identifier).

.. code-block:: arduino

    IPAddress getMeshLocalEid() const;

This function returns the mesh-local IPv6 address as an ``IPAddress`` object.

getLeaderRloc
^^^^^^^^^^^^^

Gets the Thread Leader RLOC (Routing Locator).

.. code-block:: arduino

    IPAddress getLeaderRloc() const;

This function returns the IPv6 address of the Thread Leader as an ``IPAddress`` object.

getRloc
^^^^^^^

Gets the node RLOC (Routing Locator).

.. code-block:: arduino

    IPAddress getRloc() const;

This function returns the IPv6 RLOC address of this node as an ``IPAddress`` object.

getRloc16
^^^^^^^^^

Gets the RLOC16 (16-bit Routing Locator).

.. code-block:: arduino

    uint16_t getRloc16() const;

This function returns the 16-bit RLOC of this node.

Unicast Address Management
**************************

getUnicastAddressCount
^^^^^^^^^^^^^^^^^^^^^^

Gets the number of unicast addresses.

.. code-block:: arduino

    size_t getUnicastAddressCount() const;

This function returns the number of unicast IPv6 addresses assigned to this node. The count is cached for performance.

getUnicastAddress
^^^^^^^^^^^^^^^^^

Gets a unicast address by index.

.. code-block:: arduino

    IPAddress getUnicastAddress(size_t index) const;

* ``index`` - The index of the address (0-based).

This function returns the unicast IPv6 address at the specified index as an ``IPAddress`` object.

**Note:** Addresses are cached for performance. Use ``clearUnicastAddressCache()`` to refresh the cache.

getAllUnicastAddresses
^^^^^^^^^^^^^^^^^^^^^^

Gets all unicast addresses.

.. code-block:: arduino

    std::vector<IPAddress> getAllUnicastAddresses() const;

This function returns a vector containing all unicast IPv6 addresses assigned to this node.

Multicast Address Management
****************************

getMulticastAddressCount
^^^^^^^^^^^^^^^^^^^^^^^^

Gets the number of multicast addresses.

.. code-block:: arduino

    size_t getMulticastAddressCount() const;

This function returns the number of multicast IPv6 addresses subscribed by this node. The count is cached for performance.

getMulticastAddress
^^^^^^^^^^^^^^^^^^^

Gets a multicast address by index.

.. code-block:: arduino

    IPAddress getMulticastAddress(size_t index) const;

* ``index`` - The index of the address (0-based).

This function returns the multicast IPv6 address at the specified index as an ``IPAddress`` object.

**Note:** Addresses are cached for performance. Use ``clearMulticastAddressCache()`` to refresh the cache.

getAllMulticastAddresses
^^^^^^^^^^^^^^^^^^^^^^^^

Gets all multicast addresses.

.. code-block:: arduino

    std::vector<IPAddress> getAllMulticastAddresses() const;

This function returns a vector containing all multicast IPv6 addresses subscribed by this node.

subscribeMulticast
^^^^^^^^^^^^^^^^^^

Join an IPv6 multicast group on the Thread interface so the node receives
datagrams sent to that group. Reference-counted: multiple callers (for example
``OThreadUDP.beginMulticast()`` and ``OThreadCoAPServer.joinMulticastGroup()``)
can share the same group; ``unsubscribeMulticast()`` removes one reference.
The refcount table is mutex-protected, so concurrent subscribe/unsubscribe from
``loop()`` and the CLI console task is safe.

See :doc:`Multicast guide <Multicasting>` for send vs receive, UDP/CoAP patterns,
scopes, shutdown order, and examples.

.. code-block:: arduino

    bool subscribeMulticast(const IPAddress &group);

* ``group`` — IPv6 multicast address (first byte ``0xFF``).

unsubscribeMulticast
^^^^^^^^^^^^^^^^^^^^

Leave an IPv6 multicast group previously joined with ``subscribeMulticast()``.

.. code-block:: arduino

    bool unsubscribeMulticast(const IPAddress &group);

Cache Management
****************

clearUnicastAddressCache
^^^^^^^^^^^^^^^^^^^^^^^^

Clears the unicast address cache.

.. code-block:: arduino

    void clearUnicastAddressCache() const;

This function clears the cached unicast addresses. The cache will be automatically repopulated on the next address access.

clearMulticastAddressCache
^^^^^^^^^^^^^^^^^^^^^^^^^^

Clears the multicast address cache.

.. code-block:: arduino

    void clearMulticastAddressCache() const;

This function clears the cached multicast addresses. The cache will be automatically repopulated on the next address access.

clearAllAddressCache
^^^^^^^^^^^^^^^^^^^^

Clears all address caches.

.. code-block:: arduino

    void clearAllAddressCache() const;

This function clears both unicast and multicast address caches.

Advanced Access
***************

getInstance
^^^^^^^^^^^

Gets the OpenThread instance pointer.

.. code-block:: arduino

    otInstance *getInstance();

This function returns a pointer to the underlying OpenThread instance. This allows direct access to OpenThread API functions for advanced use cases.

**Warning:** Direct use of the OpenThread instance requires knowledge of the OpenThread API. Use with caution. The Arduino wrapper methods acquire the ESP OpenThread stack lock internally, but raw ``ot*`` calls made through this pointer are outside that protection and must follow the ESP-IDF OpenThread locking rules.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns whether OpenThread is started.

.. code-block:: arduino

    operator bool() const;

This operator returns ``true`` if OpenThread is started and ready, ``false`` otherwise.

**Example:**

.. code-block:: arduino

    if (OThread) {
        Serial.println("OpenThread is ready");
    }

Example
-------

Basic Thread Network Setup
**************************

.. code-block:: arduino

    #include <OThread.h>

    void setup() {
        Serial.begin(115200);

        // Initialize OpenThread stack
        OpenThread::begin();

        // Wait for OpenThread to be ready
        while (!OThread) {
            delay(100);
        }

        // Create and configure dataset
        DataSet dataset;
        dataset.initNew();
        dataset.setNetworkName("MyThreadNetwork");
        dataset.setChannel(15);

        // Set network key (16 bytes)
        uint8_t networkKey[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        dataset.setNetworkKey(networkKey);

        // Apply dataset and start network
        OThread.commitDataSet(dataset);
        OThread.networkInterfaceUp();
        OThread.start();

        // Wait for network to be ready
        while (OpenThread::otGetDeviceRole() == OT_ROLE_DETACHED) {
            delay(100);
        }

        // Print network information
        OpenThread::otPrintNetworkInformation(Serial);

        // Get and print addresses
        Serial.printf("Mesh Local EID: %s\r\n", OThread.getMeshLocalEid().toString().c_str());
        Serial.printf("RLOC: %s\r\n", OThread.getRloc().toString().c_str());
        Serial.printf("RLOC16: 0x%04x\r\n", OThread.getRloc16());
    }

Monitoring Device Role
**********************

.. code-block:: arduino

    void loop() {
        ot_device_role_t role = OpenThread::otGetDeviceRole();
        const char *roleStr = OpenThread::otGetStringDeviceRole();

        Serial.printf("Current role: %s\r\n", roleStr);

        switch (role) {
            case OT_ROLE_LEADER:
                Serial.println("This device is the Thread Leader");
                break;
            case OT_ROLE_ROUTER:
                Serial.println("This device is a Thread Router");
                break;
            case OT_ROLE_CHILD:
                Serial.println("This device is a Thread Child");
                break;
            case OT_ROLE_DETACHED:
                Serial.println("This device is not attached to a network");
                break;
            case OT_ROLE_DISABLED:
                Serial.println("Thread is disabled");
                break;
        }

        delay(5000);
    }

Address Management
******************

.. code-block:: arduino

    void printAddresses() {
        // Print unicast addresses
        size_t unicastCount = OThread.getUnicastAddressCount();
        Serial.printf("Unicast addresses: %lu\r\n", (unsigned long)unicastCount);
        for (size_t i = 0; i < unicastCount; i++) {
            Serial.printf("  [%lu] %s\r\n", (unsigned long)i, OThread.getUnicastAddress(i).toString().c_str());
        }

        // Print multicast addresses
        size_t multicastCount = OThread.getMulticastAddressCount();
        Serial.printf("Multicast addresses: %lu\r\n", (unsigned long)multicastCount);
        for (size_t i = 0; i < multicastCount; i++) {
            Serial.printf("  [%lu] %s\r\n", (unsigned long)i, OThread.getMulticastAddress(i).toString().c_str());
        }

        // Clear cache to force refresh
        OThread.clearAllAddressCache();
    }
