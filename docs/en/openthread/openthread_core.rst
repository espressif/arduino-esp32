################
OpenThread Class
################

About
-----

The ``OpenThread`` class provides direct access to OpenThread API functions for managing Thread network operations. This is the **Classes API** approach, which offers object-oriented methods that directly call OpenThread API functions.

**Key Features:**
* Direct OpenThread API access
* Network management (start, stop, interface control)
* Dataset management
* Address management with caching
* Network information retrieval
* Device role monitoring

**Use Cases:**
* Thread network configuration and management
* Direct control over Thread operations
* Programmatic network setup
* Address and routing information access

API Reference
-------------

Initialization
**************

begin
^^^^^

Initializes the OpenThread stack.

.. code-block:: arduino

    static void begin(bool OThreadAutoStart = true);

* ``OThreadAutoStart`` - If ``true``, automatically starts Thread with default dataset from NVS or ESP-IDF settings (default: ``true``)

This function initializes the OpenThread stack and creates the OpenThread task. If ``OThreadAutoStart`` is ``true``, it will attempt to start Thread using the active dataset from NVS or ESP-IDF default settings.

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

This function disables the Thread IPv6 interface (equivalent to CLI command ``ifconfig down``). The device will stop sending and receiving IPv6 packets.

Dataset Management
******************

commitDataSet
^^^^^^^^^^^^^

Commits an operational dataset to the Thread network.

.. code-block:: arduino

    void commitDataSet(const DataSet &dataset);

* ``dataset`` - The ``DataSet`` object containing the operational dataset parameters

This function sets the active operational dataset for the Thread network. The dataset must be properly configured before committing.

**Example:**

.. code-block:: arduino

    DataSet dataset;
    dataset.initNew();
    dataset.setNetworkName("MyThreadNetwork");
    dataset.setChannel(15);
    dataset.setNetworkKey(networkKey);
    OThread.commitDataSet(dataset);

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

Device Role
***********

otGetDeviceRole
^^^^^^^^^^^^^^^

Gets the current device role.

.. code-block:: arduino

    static ot_device_role_t otGetDeviceRole();

This function returns the current Thread device role:

* ``OT_ROLE_DISABLED`` - The Thread stack is disabled
* ``OT_ROLE_DETACHED`` - Not currently participating in a Thread network
* ``OT_ROLE_CHILD`` - The Thread Child role
* ``OT_ROLE_ROUTER`` - The Thread Router role
* ``OT_ROLE_LEADER`` - The Thread Leader role

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

* ``output`` - The Stream object to print to (e.g., ``Serial``)

This function prints comprehensive network information including:
* Device role
* RLOC16
* Network name
* Channel
* PAN ID
* Extended PAN ID
* Network key

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

* ``index`` - The index of the address (0-based)

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

* ``index`` - The index of the address (0-based)

This function returns the multicast IPv6 address at the specified index as an ``IPAddress`` object.

**Note:** Addresses are cached for performance. Use ``clearMulticastAddressCache()`` to refresh the cache.

getAllMulticastAddresses
^^^^^^^^^^^^^^^^^^^^^^^^

Gets all multicast addresses.

.. code-block:: arduino

    std::vector<IPAddress> getAllMulticastAddresses() const;

This function returns a vector containing all multicast IPv6 addresses subscribed by this node.

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

**Warning:** Direct use of the OpenThread instance requires knowledge of the OpenThread API. Use with caution.

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
        OThread.start();
        OThread.networkInterfaceUp();

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
