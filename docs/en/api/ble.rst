#######
BLE API
#######

About
-----

The BLE (Bluetooth Low Energy) library provides a unified, stack-agnostic API for Bluetooth Low Energy on ESP32 family chips.
It supports both the **NimBLE** and **Bluedroid** host stacks transparently -- the same application code compiles and runs
on either backend without ``#ifdef`` guards.

Key Features:

* **Stack-agnostic API** -- same code compiles for NimBLE and Bluedroid
* **GATT Server and Client** with shared-handle semantics (PIMPL pattern)
* **BLE 5.0** extended advertising, periodic advertising, PHY selection, Data Length Extension
* **Security**: pairing, bonding, MITM protection, Secure Connections (LE SC)
* **HID-over-GATT** (HOGP) device support via ``BLEHIDDevice``
* **Beacon support**: Apple iBeacon and Google Eddystone (URL + TLM)
* **Hosted BLE** for ESP32-P4 via *esp-hosted* (runs NimBLE on a co-processor)
* **L2CAP CoC** channels for high-throughput bulk data transfer (NimBLE, ``BLE_L2CAP_SUPPORTED``)
* **BLEStream**: Arduino ``Stream`` interface over BLE using the Nordic UART Service (NUS)
* **Modern C++17 API**: ``std::function`` callbacks, RAII handles, ``BTStatus`` error handling

Supported SoCs
**************

The BLE library is available on: **ESP32**, **ESP32-S3**, **ESP32-C3**, **ESP32-C6**, **ESP32-C5**, **ESP32-H2**, and **ESP32-P4** (via hosted BLE).

BLE 5.0 features (extended advertising, periodic advertising, PHY selection, DLE) require a BLE 5.0-capable SoC
(ESP32-S3, ESP32-C3, ESP32-C6, ESP32-C5, ESP32-H2, ESP32-P4) and are compile-time guarded by ``SOC_BLE_50_SUPPORTED``.

Architecture and Design
-----------------------

PIMPL Pattern (Handle Types)
*****************************

Public classes such as ``BLEServer``, ``BLEClient``, ``BLEService``, ``BLECharacteristic``, ``BLEDescriptor``,
``BLEScan``, ``BLEAdvertising``, ``BLESecurity``, ``BLEAdvertisedDevice``, ``BLERemoteService``,
``BLERemoteCharacteristic``, and ``BLERemoteDescriptor`` are lightweight *shared handles* wrapping a
``std::shared_ptr<Impl>``.

* **Copying** a handle creates shared ownership (reference count increment).
* **Moving** a handle transfers ownership (source becomes null).
* A **default-constructed** handle is null -- test with ``explicit operator bool()``:

.. code-block:: cpp

    BLEServer server;       // null handle
    if (!server) {
        Serial.println("Handle is null");
    }
    server = BLE.createServer();  // now valid
    BLEServer alias = server;     // shared ownership

Value Types vs Handle Types
***************************

Not every class uses the PIMPL pattern. The following are **value types** -- they are copyable, have no
``shared_ptr``, and behave like plain C++ structs/classes:

* ``BTAddress`` -- 6-byte Bluetooth address with type tag
* ``BLEUUID`` -- 16/32/128-bit UUID
* ``BLEConnInfo`` -- connection descriptor (inline storage)
* ``BLEConnParams`` -- connection parameters struct
* ``BLEAdvertisementData`` -- advertisement payload builder
* ``BLEScanResults`` -- vector of ``BLEAdvertisedDevice``
* ``BTStatus`` -- 1-byte error code with boolean conversion

Stack Selection
***************

The BLE host stack is chosen at **build time** via ``sdkconfig``:

* ``CONFIG_NIMBLE_ENABLED`` selects NimBLE
* ``CONFIG_BLUEDROID_ENABLED`` selects Bluedroid

The public API is identical regardless of the backend. Backend-specific code lives entirely in the
``impl/nimble/`` and ``impl/bluedroid/`` directories and is never included by user code.

Thread Safety
*************

* User code typically runs on the **Arduino task** (``loop()``).
* Stack callbacks run on the **BT host task**.
* The library uses FreeRTOS task notifications (``BLESync``) for blocking operations (connect, discover, etc.).
* User callbacks (``onConnect``, ``onWrite``, etc.) are invoked **from the BT host task** -- keep them fast and
  avoid blocking calls.

.. warning::

    User callbacks execute on the BT host task, not the Arduino task. Long-running work inside a callback
    will stall the Bluetooth stack. Use flags or queues to defer heavy work to ``loop()``.

UUID Byte Order
***************

``BLEUUID`` stores 128-bit UUIDs in **big-endian** (network / display) order internally.
Backend implementations convert to the stack-native byte order (little-endian for NimBLE) as needed.
You never need to worry about byte order when using the public API.

Error Handling
**************

All operations that can fail return ``BTStatus``. Check results with the boolean conversion:

.. code-block:: cpp

    BTStatus status = BLE.begin("MyDevice");
    if (!status) {
        Serial.printf("BLE init failed: %s\n", status.toString());
    }

    if (status == BTStatus::Timeout) {
        Serial.println("Operation timed out");
    }

BLE 5.0 Features
*****************

Extended advertising, periodic advertising, PHY selection, and Data Length Extension are available on
SoCs with ``SOC_BLE_50_SUPPORTED`` (or via hosted BLE). These features are compile-time guarded by the
``BLE5_SUPPORTED`` macro defined in ``impl/BLEGuards.h``.

Getting Started
---------------

1. Include ``<BLE.h>`` -- this single header provides access to the global ``BLE`` object and all public
   BLE classes (``BLEServer``, ``BLEClient``, ``BLEScan``, ``BLECharacteristic``, ``BLEBeacon``, etc.).
2. Call ``BLE.begin()`` to initialize the stack.
3. Create a server, client, scanner, or advertiser as needed.

.. code-block:: cpp

    #include <BLE.h>

    void setup() {
        Serial.begin(115200);

        if (!BLE.begin("MyDevice")) {
            Serial.println("BLE init failed!");
            return;
        }

        // Create a GATT server
        BLEServer server = BLE.createServer();
        BLEService svc = server.createService("180A");
        BLECharacteristic chr = svc.createCharacteristic(
            "2A29", BLEProperty::Read, BLEPermissions::OpenRead
        );
        chr.setValue("Espressif");
        server.start();

        BLE.startAdvertising();
        Serial.println("BLE server started");
    }

    void loop() {
        delay(1000);
    }

BLEClass (Global BLE Object)
-----------------------------

The ``BLE`` global singleton is the entry point for all BLE operations. The class is ``BLEClass``; users
interact via the pre-instantiated ``BLE`` object.

Lifecycle
*********

begin
^^^^^

Initialize the BLE stack with an optional device name.

.. code-block:: cpp

    BTStatus begin(const String &deviceName = "");

Returns ``BTStatus::OK`` on success.

end
^^^

Deinitialize the BLE stack. Pass ``true`` to release controller memory (cannot re-initialize after).

.. code-block:: cpp

    void end(bool releaseMemory = false);

isInitialized
^^^^^^^^^^^^^

.. code-block:: cpp

    bool isInitialized() const;

Returns ``true`` if ``begin()`` was called successfully and ``end()`` has not been called.

operator bool
^^^^^^^^^^^^^

.. code-block:: cpp

    explicit operator bool() const;

Equivalent to ``isInitialized()``.

Identity
********

getAddress
^^^^^^^^^^

.. code-block:: cpp

    BTAddress getAddress() const;

Returns the local Bluetooth address.

getDeviceName
^^^^^^^^^^^^^

.. code-block:: cpp

    String getDeviceName() const;

Returns the device name set during ``begin()``.

setOwnAddressType
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus setOwnAddressType(BTAddress::Type type);

Set the address type used for advertising and connections (``Public``, ``Random``, ``PublicID``, ``RandomID``).

getOwnAddressType
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTAddress::Type getOwnAddressType() const;

Returns the currently configured own-address type.

setOwnAddress
^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus setOwnAddress(const BTAddress &addr);

Set a custom local address.

Factory Methods
***************

createServer
^^^^^^^^^^^^

.. code-block:: cpp

    BLEServer createServer();

Returns a handle to the GATT server singleton. Idempotent -- always returns the same server.

createClient
^^^^^^^^^^^^

.. code-block:: cpp

    BLEClient createClient();

Creates a new GATT client instance. Each client manages one connection. Multiple clients can coexist.

getScan
^^^^^^^

.. code-block:: cpp

    BLEScan getScan();

Returns the scanner singleton handle.

getAdvertising
^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEAdvertising getAdvertising();

Returns the advertising controller singleton handle.

getSecurity
^^^^^^^^^^^

.. code-block:: cpp

    BLESecurity getSecurity();

Returns the security manager singleton handle.

Power
*****

setPower
^^^^^^^^

.. code-block:: cpp

    void setPower(int8_t txPowerDbm);

Set the transmit power in dBm.

getPower
^^^^^^^^

.. code-block:: cpp

    int8_t getPower() const;

Get the current transmit power in dBm.

MTU
***

setMTU
^^^^^^

.. code-block:: cpp

    BTStatus setMTU(uint16_t mtu);

Set the preferred ATT MTU size. The actual MTU is negotiated per connection.

getMTU
^^^^^^

.. code-block:: cpp

    uint16_t getMTU() const;

Get the configured preferred MTU.

IRK (Identity Resolving Key)
*****************************

Useful for integrations with Home Assistant, ESPresense, and other systems that track devices by IRK.

.. code-block:: cpp

    bool getLocalIRK(uint8_t irk[16]) const;
    String getLocalIRKString() const;
    String getLocalIRKBase64() const;
    bool getPeerIRK(const BTAddress &peer, uint8_t irk[16]) const;
    String getPeerIRKString(const BTAddress &peer) const;
    String getPeerIRKBase64(const BTAddress &peer) const;
    String getPeerIRKReverse(const BTAddress &peer) const;

BLE 5.0 Default PHY
********************

setDefaultPhy
^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus setDefaultPhy(BLEPhy txPhy, BLEPhy rxPhy);

Set the default PHY preference for new connections.

getDefaultPhy
^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus getDefaultPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const;

Read the current default PHY settings.

Whitelist
*********

.. code-block:: cpp

    BTStatus whiteListAdd(const BTAddress &address);
    BTStatus whiteListRemove(const BTAddress &address);
    bool isOnWhiteList(const BTAddress &address) const;

Advertising Shortcuts
*********************

.. code-block:: cpp

    BTStatus startAdvertising();
    BTStatus stopAdvertising();

Convenience methods that forward to the advertising singleton.

Stack Info
**********

.. code-block:: cpp

    enum class Stack { NimBLE, Bluedroid };
    Stack getStack() const;
    const char *getStackName() const;
    bool isHostedBLE() const;

Hosted BLE (ESP32-P4)
*********************

.. code-block:: cpp

    BTStatus setPins(int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst);

Configure SDIO pins for esp-hosted BLE co-processor communication. Call before ``begin()``.

Raw Event Handlers (Advanced)
*****************************

.. code-block:: cpp

    using RawEventHandler = std::function<int(void *event, void *arg)>;
    BTStatus setCustomGapHandler(RawEventHandler handler);
    BTStatus setCustomGattcHandler(RawEventHandler handler);
    BTStatus setCustomGattsHandler(RawEventHandler handler);

Register custom handlers for raw GAP, GATT client, and GATT server events. These are advanced extension
points for features not directly exposed by the library.

.. warning::

    Raw event handler signatures and event structures are stack-specific. Your handler code will be
    tied to the selected backend (NimBLE or Bluedroid).

GATT Server
-----------

BLEServer
*********

Created via ``BLE.createServer()``. Represents the local GATT server.

.. code-block:: cpp

    BLEServer server = BLE.createServer();

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEServer();                           // default: null handle
    explicit operator bool() const;        // true if valid

Service Management
^^^^^^^^^^^^^^^^^^

createService
"""""""""""""

.. code-block:: cpp

    BLEService createService(const BLEUUID &uuid, uint32_t numHandles = 15, uint8_t instId = 0);

Create a GATT service. ``numHandles`` reserves attribute handles for included characteristics and
descriptors (default 15 is sufficient for most services). ``instId`` differentiates multiple instances
of the same UUID.

getService
""""""""""

.. code-block:: cpp

    BLEService getService(const BLEUUID &uuid);

Retrieve a service by UUID. Returns a null handle if not found.

getServices
"""""""""""

.. code-block:: cpp

    std::vector<BLEService> getServices() const;

Returns all registered services.

removeService
"""""""""""""

.. code-block:: cpp

    BTStatus removeService(const BLEService &service);

Remove a previously created service.

Server Lifecycle
^^^^^^^^^^^^^^^^

start
"""""

.. code-block:: cpp

    BTStatus start();

Register all services with the stack and start the GATT server. Call after creating services and
characteristics.

isStarted
"""""""""

.. code-block:: cpp

    bool isStarted() const;

Callbacks
^^^^^^^^^

All callback setters accept ``std::function`` handlers.

onConnect
"""""""""

.. code-block:: cpp

    using ConnectHandler = std::function<void(BLEServer server, const BLEConnInfo &conn)>;
    void onConnect(ConnectHandler handler);

Called when a remote device connects to this server.

onDisconnect
"""""""""""""

.. code-block:: cpp

    using DisconnectHandler = std::function<void(BLEServer server, const BLEConnInfo &conn, uint8_t reason)>;
    void onDisconnect(DisconnectHandler handler);

Called when a remote device disconnects. ``reason`` is the HCI disconnect reason code (e.g. ``0x13`` = remote
user terminated).

onMtuChanged
"""""""""""""

.. code-block:: cpp

    using MtuChangedHandler = std::function<void(BLEServer server, const BLEConnInfo &conn, uint16_t mtu)>;
    void onMtuChanged(MtuChangedHandler handler);

Called when the ATT MTU is renegotiated for a connection.

onConnParamsUpdate
""""""""""""""""""

.. code-block:: cpp

    using ConnParamsHandler = std::function<void(BLEServer server, const BLEConnInfo &conn)>;
    void onConnParamsUpdate(ConnParamsHandler handler);

Called when connection parameters are updated.

onIdentity
""""""""""

.. code-block:: cpp

    using IdentityHandler = std::function<void(BLEServer server, const BLEConnInfo &conn)>;
    void onIdentity(IdentityHandler handler);

Called when a peer's identity address is resolved (e.g. after bonding with IRK exchange).

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

Advertising Control
^^^^^^^^^^^^^^^^^^^

advertiseOnDisconnect
"""""""""""""""""""""

.. code-block:: cpp

    void advertiseOnDisconnect(bool enable);

When enabled (default), the server automatically restarts advertising after a client disconnects.

getAdvertising
""""""""""""""

.. code-block:: cpp

    BLEAdvertising getAdvertising();

Get the advertising controller associated with this server.

startAdvertising / stopAdvertising
""""""""""""""""""""""""""""""""""

.. code-block:: cpp

    BTStatus startAdvertising();
    BTStatus stopAdvertising();

Connection Management
^^^^^^^^^^^^^^^^^^^^^

getConnectedCount
"""""""""""""""""

.. code-block:: cpp

    size_t getConnectedCount() const;

Returns the number of currently connected clients.

getConnections
""""""""""""""

.. code-block:: cpp

    std::vector<BLEConnInfo> getConnections() const;

Returns connection information for all connected clients.

disconnect
""""""""""

.. code-block:: cpp

    BTStatus disconnect(uint16_t connHandle, uint8_t reason = 0x13);

Disconnect a specific client by connection handle.

connect
"""""""

.. code-block:: cpp

    BTStatus connect(const BTAddress &address);

Initiate a connection to a peripheral (server acting as central).

getPeerMTU
""""""""""

.. code-block:: cpp

    uint16_t getPeerMTU(uint16_t connHandle) const;

Get the negotiated MTU for a specific connection.

updateConnParams
""""""""""""""""

.. code-block:: cpp

    BTStatus updateConnParams(uint16_t connHandle, const BLEConnParams &params);

Request a connection parameter update for a specific connection.

setPhy / getPhy
"""""""""""""""

.. code-block:: cpp

    BTStatus setPhy(uint16_t connHandle, BLEPhy txPhy, BLEPhy rxPhy);
    BTStatus getPhy(uint16_t connHandle, BLEPhy &txPhy, BLEPhy &rxPhy) const;

Set or query the PHY for a specific connection (BLE 5.0).

setDataLen
""""""""""

.. code-block:: cpp

    BTStatus setDataLen(uint16_t connHandle, uint16_t txOctets, uint16_t txTime);

Set the Data Length Extension parameters for a specific connection.

BLEService
**********

Created via ``server.createService(uuid)``. Represents a GATT service.

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEService();                          // default: null handle
    explicit operator bool() const;        // true if valid

Included Services
^^^^^^^^^^^^^^^^^

addIncludedService
""""""""""""""""""

.. code-block:: cpp

    void addIncludedService(const BLEService &service);

Declare another service as an *Included Service* of this one (GATT §3.2). The included
service must be created before the server is started. Used by ``BLEHIDDevice`` to include
the Battery Service in the HID Service per HIDS 1.0 §3.

Characteristic Management
^^^^^^^^^^^^^^^^^^^^^^^^^

createCharacteristic
""""""""""""""""""""

.. code-block:: cpp

    BLECharacteristic createCharacteristic(const BLEUUID &uuid,
                                           BLEProperty properties,
                                           BLEPermission permissions);

Create a characteristic with the given UUID, properties and permissions.

Permissions are required. The mapping is fail-closed: a read or write
property is only exposed if the matching permission direction is declared.
Use a preset from the ``BLEPermissions::`` namespace (for example
``BLEPermissions::OpenReadWrite`` or ``BLEPermissions::EncryptedRead``) for
the common cases, or combine raw ``BLEPermission`` bits for custom setups.
For notify- or indicate-only characteristics pass ``BLEPermission::None``.

getCharacteristic
"""""""""""""""""

.. code-block:: cpp

    BLECharacteristic getCharacteristic(const BLEUUID &uuid);

getCharacteristics
""""""""""""""""""

.. code-block:: cpp

    std::vector<BLECharacteristic> getCharacteristics() const;

removeCharacteristic
""""""""""""""""""""

.. code-block:: cpp

    void removeCharacteristic(const BLECharacteristic &chr);

Service Lifecycle
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool isStarted() const;

``isStarted()`` returns ``true`` after the parent server's ``start()`` has been called, registering
the service in the GATT database.

Accessors
^^^^^^^^^

.. code-block:: cpp

    BLEUUID getUUID() const;
    uint16_t getHandle() const;
    BLEServer getServer() const;

BLECharacteristic
*****************

Created via ``service.createCharacteristic(uuid, properties)``. Represents a GATT characteristic.

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLECharacteristic();                   // default: null handle
    explicit operator bool() const;        // true if valid

NotifyStatus Enum
^^^^^^^^^^^^^^^^^

Reported by the ``StatusHandler`` callback:

.. code-block:: cpp

    enum NotifyStatus {
        SUCCESS_INDICATE,
        SUCCESS_NOTIFY,
        ERROR_INDICATE_DISABLED,
        ERROR_NOTIFY_DISABLED,
        ERROR_GATT,
        ERROR_NO_CLIENT,
        ERROR_NO_SUBSCRIBER,
        ERROR_INDICATE_TIMEOUT,
        ERROR_INDICATE_FAILURE,
    };

Callbacks
^^^^^^^^^

onRead
""""""

.. code-block:: cpp

    using ReadHandler = std::function<void(BLECharacteristic chr, const BLEConnInfo &conn)>;
    void onRead(ReadHandler handler);

Called before a client reads the characteristic value. Use this to update the value dynamically.

onWrite
"""""""

.. code-block:: cpp

    using WriteHandler = std::function<void(BLECharacteristic chr, const BLEConnInfo &conn)>;
    void onWrite(WriteHandler handler);

Called after a client writes to the characteristic. Retrieve the new value with ``getValue()``.

onNotify
""""""""

.. code-block:: cpp

    using NotifyHandler = std::function<void(BLECharacteristic chr)>;
    void onNotify(NotifyHandler handler);

Called when a notification is sent.

onSubscribe
"""""""""""

.. code-block:: cpp

    using SubscribeHandler = std::function<void(BLECharacteristic chr, const BLEConnInfo &conn, uint16_t subValue)>;
    void onSubscribe(SubscribeHandler handler);

Called when a client subscribes to or unsubscribes from notifications/indications. ``subValue`` indicates
the subscription type (0 = unsubscribe, 1 = notifications, 2 = indications).

onStatus
""""""""

.. code-block:: cpp

    using StatusHandler = std::function<void(BLECharacteristic chr, NotifyStatus status, uint32_t code)>;
    void onStatus(StatusHandler handler);

Called with delivery status after a notification or indication is sent.

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

Value Access
^^^^^^^^^^^^

setValue
""""""""

.. code-block:: cpp

    void setValue(const uint8_t *data, size_t length);
    void setValue(const String &value);
    void setValue(uint16_t value);
    void setValue(uint32_t value);
    void setValue(int value);
    void setValue(float value);
    void setValue(double value);

    template<typename T>
    void setValue(const T &value);

Set the characteristic value. The template overload writes the raw bytes of any POD type.

getValue
""""""""

.. code-block:: cpp

    const uint8_t *getValue(size_t *length = nullptr) const;
    String getStringValue() const;

    template<typename T>
    T getValue() const;

Read the characteristic value. The pointer returned by ``getValue()`` is valid until the next ``setValue()``
call. The template overload reinterprets the raw bytes as type ``T``.

Notifications and Indications
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

notify
""""""

.. code-block:: cpp

    BTStatus notify(const uint8_t *data = nullptr, size_t length = 0);
    BTStatus notify(uint16_t connHandle, const uint8_t *data = nullptr, size_t length = 0);

Send a notification to all subscribers or to a specific connection. If ``data`` is ``nullptr``, the current
characteristic value is sent.

indicate
""""""""

.. code-block:: cpp

    BTStatus indicate(const uint8_t *data = nullptr, size_t length = 0);
    BTStatus indicate(uint16_t connHandle, const uint8_t *data = nullptr, size_t length = 0);

Send an indication to all subscribers or to a specific connection. Indications require acknowledgment
from the client.

Properties and Permissions
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEProperty getProperties() const;
    void setPermissions(BLEPermission permissions);
    BLEPermission getPermissions() const;

Descriptor Management
^^^^^^^^^^^^^^^^^^^^^

createDescriptor
""""""""""""""""

.. code-block:: cpp

    BLEDescriptor createDescriptor(const BLEUUID &uuid, BLEPermission perms = BLEPermission::Read, size_t maxLen = 100);

Create a descriptor under this characteristic.

getDescriptor
"""""""""""""

.. code-block:: cpp

    BLEDescriptor getDescriptor(const BLEUUID &uuid);

getDescriptors
""""""""""""""

.. code-block:: cpp

    std::vector<BLEDescriptor> getDescriptors() const;

removeDescriptor
""""""""""""""""

.. code-block:: cpp

    void removeDescriptor(const BLEDescriptor &desc);

Subscription State
^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    size_t getSubscribedCount() const;
    std::vector<uint16_t> getSubscribedConnections() const;
    bool isSubscribed(uint16_t connHandle) const;

Accessors
^^^^^^^^^

.. code-block:: cpp

    BLEUUID getUUID() const;
    uint16_t getHandle() const;
    BLEService getService() const;
    void setDescription(const String &desc);
    String toString() const;

``setDescription()`` is a convenience that creates or updates a User Description descriptor (0x2901).

BLEDescriptor
*************

Created via ``characteristic.createDescriptor(uuid)`` or static factory methods.

Constructors
^^^^^^^^^^^^

.. code-block:: cpp

    BLEDescriptor();                                         // null handle
    BLEDescriptor(const BLEUUID &uuid, uint16_t maxLength = 100);  // standalone descriptor
    explicit operator bool() const;

Callbacks
^^^^^^^^^

.. code-block:: cpp

    using ReadHandler = std::function<void(BLEDescriptor desc, const BLEConnInfo &conn)>;
    using WriteHandler = std::function<void(BLEDescriptor desc, const BLEConnInfo &conn)>;
    void onRead(ReadHandler handler);
    void onWrite(WriteHandler handler);

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

Value Access
^^^^^^^^^^^^

.. code-block:: cpp

    void setValue(const uint8_t *data, size_t length);
    void setValue(const String &value);
    const uint8_t *getValue(size_t *length = nullptr) const;
    size_t getLength() const;

Permissions
^^^^^^^^^^^

.. code-block:: cpp

    void setPermissions(BLEPermission perms);

Accessors
^^^^^^^^^

.. code-block:: cpp

    BLEUUID getUUID() const;
    uint16_t getHandle() const;
    BLECharacteristic getCharacteristic() const;
    String toString() const;

Static Factories
^^^^^^^^^^^^^^^^

createUserDescription
"""""""""""""""""""""

.. code-block:: cpp

    static BLEDescriptor createUserDescription(const String &description);

Creates a Characteristic User Description descriptor (UUID 0x2901).

createCCCD
""""""""""

.. code-block:: cpp

    static BLEDescriptor createCCCD();

Creates a Client Characteristic Configuration Descriptor (UUID 0x2902).

.. note::

    A CCCD is automatically created for characteristics with ``Notify`` or ``Indicate`` properties.
    You rarely need to create one manually.

createPresentationFormat
""""""""""""""""""""""""

.. code-block:: cpp

    static BLEDescriptor createPresentationFormat();

Creates a Characteristic Presentation Format descriptor (UUID 0x2904).

User Description Convenience (0x2901)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    void setUserDescription(const String &description);
    String getUserDescription() const;

CCCD Convenience (0x2902)
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool getNotifications() const;
    bool getIndications() const;
    void setNotifications(bool enable);
    void setIndications(bool enable);

Presentation Format Convenience (0x2904)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    void setFormat(uint8_t format);
    void setExponent(int8_t exponent);
    void setUnit(uint16_t unit);
    void setNamespace(uint8_t ns);
    void setFormatDescription(uint16_t description);

FORMAT Constants
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    static constexpr uint8_t FORMAT_BOOLEAN = 1;
    static constexpr uint8_t FORMAT_UINT8   = 4;
    static constexpr uint8_t FORMAT_UINT16  = 6;
    static constexpr uint8_t FORMAT_UINT32  = 8;
    static constexpr uint8_t FORMAT_SINT8   = 12;
    static constexpr uint8_t FORMAT_SINT16  = 14;
    static constexpr uint8_t FORMAT_SINT32  = 16;
    static constexpr uint8_t FORMAT_FLOAT32 = 20;
    static constexpr uint8_t FORMAT_FLOAT64 = 21;
    static constexpr uint8_t FORMAT_UTF8    = 25;
    static constexpr uint8_t FORMAT_UTF16   = 26;

Type Queries
^^^^^^^^^^^^

.. code-block:: cpp

    bool isUserDescription() const;    // UUID == 0x2901
    bool isCCCD() const;               // UUID == 0x2902
    bool isPresentationFormat() const;  // UUID == 0x2904

GATT Client
-----------

BLEClient
*********

Created via ``BLE.createClient()``. Each instance manages one connection to a remote peripheral.
Multiple ``BLEClient`` instances can coexist for multi-connection scenarios.

.. code-block:: cpp

    BLEClient client = BLE.createClient();

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEClient();                           // default: null handle
    explicit operator bool() const;        // true if valid

Connecting
^^^^^^^^^^

connect
"""""""

.. code-block:: cpp

    BTStatus connect(const BTAddress &address, uint32_t timeoutMs = 5000);
    BTStatus connect(const BLEAdvertisedDevice &device, uint32_t timeoutMs = 5000);
    BTStatus connect(const BTAddress &address, BLEPhy phy, uint32_t timeoutMs = 5000);
    BTStatus connect(const BLEAdvertisedDevice &device, BLEPhy phy, uint32_t timeoutMs = 5000);

Synchronous connect. Blocks until the connection is established or the timeout expires.
The PHY-aware overloads (BLE 5.0) specify the preferred PHY for the initial connection.

connectAsync
"""""""""""""

.. code-block:: cpp

    BTStatus connectAsync(const BTAddress &address, BLEPhy phy = BLEPhy::PHY_1M);
    BTStatus connectAsync(const BLEAdvertisedDevice &device, BLEPhy phy = BLEPhy::PHY_1M);

Initiate a connection without blocking. Use the ``onConnect`` callback to know when the connection
is established.

cancelConnect
"""""""""""""

.. code-block:: cpp

    BTStatus cancelConnect();

Cancel a pending connection attempt.

disconnect
""""""""""

.. code-block:: cpp

    BTStatus disconnect();

Disconnect from the remote device.

isConnected
"""""""""""

.. code-block:: cpp

    bool isConnected() const;

secureConnection
""""""""""""""""

.. code-block:: cpp

    BTStatus secureConnection();

Initiate security/pairing on the existing connection.

Service Discovery
^^^^^^^^^^^^^^^^^

getService
""""""""""

.. code-block:: cpp

    BLERemoteService getService(const BLEUUID &uuid);

Discover and return a specific service by UUID. Triggers service discovery if not already cached.

getServices
"""""""""""

.. code-block:: cpp

    std::vector<BLERemoteService> getServices() const;

Returns all discovered services.

discoverServices
""""""""""""""""

.. code-block:: cpp

    BTStatus discoverServices();

Explicitly trigger GATT service discovery. Normally called automatically by ``getService()``.

Convenience Read/Write
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    String getValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID);
    BTStatus setValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const String &value);

Read or write a characteristic value by service and characteristic UUID in a single call.

Callbacks
^^^^^^^^^

.. code-block:: cpp

    using ConnectHandler = std::function<void(BLEClient client, const BLEConnInfo &conn)>;
    using DisconnectHandler = std::function<void(BLEClient client, const BLEConnInfo &conn, uint8_t reason)>;
    using ConnectFailHandler = std::function<void(BLEClient client, int reason)>;
    using MtuChangedHandler = std::function<void(BLEClient client, const BLEConnInfo &conn, uint16_t mtu)>;
    using ConnParamsReqHandler = std::function<bool(BLEClient client, const BLEConnParams &params)>;
    using IdentityHandler = std::function<void(BLEClient client, const BLEConnInfo &conn)>;

    void onConnect(ConnectHandler handler);
    void onDisconnect(DisconnectHandler handler);
    void onConnectFail(ConnectFailHandler handler);
    void onMtuChanged(MtuChangedHandler handler);
    void onConnParamsUpdateRequest(ConnParamsReqHandler handler);
    void onIdentity(IdentityHandler handler);

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

.. note::

    ``onConnParamsUpdateRequest`` returns ``bool`` -- return ``true`` to accept the parameters, ``false``
    to reject.

MTU and Connection Info
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    void setMTU(uint16_t mtu);
    uint16_t getMTU() const;
    int8_t getRSSI() const;
    BTAddress getPeerAddress() const;
    uint16_t getHandle() const;
    BLEConnInfo getConnection() const;

Connection Parameters and PHY
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus updateConnParams(const BLEConnParams &params);
    BTStatus setPhy(BLEPhy txPhy, BLEPhy rxPhy);
    BTStatus getPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const;
    BTStatus setDataLen(uint16_t txOctets, uint16_t txTime);

toString
^^^^^^^^

.. code-block:: cpp

    String toString() const;

BLERemoteService
****************

Represents a GATT service discovered on a remote device. Obtained via ``BLEClient::getService()``
or ``BLEClient::getServices()``.

.. code-block:: cpp

    BLERemoteService();                    // default: null handle
    explicit operator bool() const;

Characteristic Access
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLERemoteCharacteristic getCharacteristic(const BLEUUID &uuid);
    std::vector<BLERemoteCharacteristic> getCharacteristics() const;

Convenience Read/Write
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    String getValue(const BLEUUID &charUUID);
    BTStatus setValue(const BLEUUID &charUUID, const String &value);

Accessors
^^^^^^^^^

.. code-block:: cpp

    BLEClient getClient() const;
    BLEUUID getUUID() const;
    uint16_t getHandle() const;
    String toString() const;

BLERemoteCharacteristic
***********************

Represents a GATT characteristic on a remote device. Obtained via ``BLERemoteService::getCharacteristic()``.

.. code-block:: cpp

    BLERemoteCharacteristic();             // default: null handle
    explicit operator bool() const;

Reading Values
^^^^^^^^^^^^^^

.. code-block:: cpp

    String readValue(uint32_t timeoutMs = 3000);
    uint8_t readUInt8(uint32_t timeoutMs = 3000);
    uint16_t readUInt16(uint32_t timeoutMs = 3000);
    uint32_t readUInt32(uint32_t timeoutMs = 3000);
    float readFloat(uint32_t timeoutMs = 3000);
    size_t readValue(uint8_t *buf, size_t bufLen, uint32_t timeoutMs = 3000);
    const uint8_t *readRawData(size_t *len = nullptr);

The typed read methods (``readUInt8``, ``readFloat``, etc.) perform a remote GATT read and interpret the
result. ``readRawData()`` returns the cached raw bytes from the last read without triggering a new one.

Writing Values
^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus writeValue(const uint8_t *data, size_t len, bool withResponse = true);
    BTStatus writeValue(const String &value, bool withResponse = true);
    BTStatus writeValue(uint8_t value, bool withResponse = true);

Set ``withResponse`` to ``false`` for write-without-response (faster, no acknowledgment).

Capability Checks
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool canRead() const;
    bool canWrite() const;
    bool canWriteNoResponse() const;
    bool canNotify() const;
    bool canIndicate() const;
    bool canBroadcast() const;

Subscribe / Unsubscribe
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    using NotifyCallback = std::function<void(BLERemoteCharacteristic chr, const uint8_t *, size_t, bool)>;
    BTStatus subscribe(bool notifications = true, NotifyCallback callback = nullptr);
    BTStatus unsubscribe();

Subscribe to notifications (``notifications = true``) or indications (``notifications = false``).
The callback receives the characteristic, data pointer, data length, and a boolean indicating
whether it was a notification (``true``) or indication (``false``).

Descriptor Access
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLERemoteDescriptor getDescriptor(const BLEUUID &uuid);
    std::vector<BLERemoteDescriptor> getDescriptors() const;

Accessors
^^^^^^^^^

.. code-block:: cpp

    BLERemoteService getRemoteService() const;
    BLEUUID getUUID() const;
    uint16_t getHandle() const;
    String toString() const;

BLERemoteDescriptor
*******************

Represents a GATT descriptor on a remote device. Obtained via ``BLERemoteCharacteristic::getDescriptor()``.

.. code-block:: cpp

    BLERemoteDescriptor();                 // default: null handle
    explicit operator bool() const;

Reading Values
^^^^^^^^^^^^^^

.. code-block:: cpp

    String readValue(uint32_t timeoutMs = 3000);
    uint8_t readUInt8(uint32_t timeoutMs = 3000);
    uint16_t readUInt16(uint32_t timeoutMs = 3000);
    uint32_t readUInt32(uint32_t timeoutMs = 3000);

Writing Values
^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus writeValue(const uint8_t *data, size_t len, bool withResponse = true);
    BTStatus writeValue(const String &value, bool withResponse = true);
    BTStatus writeValue(uint8_t value, bool withResponse = true);

Accessors
^^^^^^^^^

.. code-block:: cpp

    BLERemoteCharacteristic getRemoteCharacteristic() const;
    BLEUUID getUUID() const;
    uint16_t getHandle() const;
    String toString() const;

Scanning
--------

BLEScan
*******

Obtained via ``BLE.getScan()``. Supports legacy scanning and BLE 5.0 extended/periodic scanning.

.. code-block:: cpp

    BLEScan scan = BLE.getScan();

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEScan();                             // default: null handle
    explicit operator bool() const;

Configuration
^^^^^^^^^^^^^

setInterval
"""""""""""

.. code-block:: cpp

    void setInterval(uint16_t intervalMs);

Set the scan interval in milliseconds.

setWindow
"""""""""

.. code-block:: cpp

    void setWindow(uint16_t windowMs);

Set the scan window in milliseconds. Must be <= interval.

setActiveScan
"""""""""""""

.. code-block:: cpp

    void setActiveScan(bool active);

Enable active scanning (sends scan requests to get scan response data).

setFilterDuplicates
"""""""""""""""""""

.. code-block:: cpp

    void setFilterDuplicates(bool filter);

When enabled, duplicate advertisements from the same device are filtered out.

clearDuplicateCache
"""""""""""""""""""

.. code-block:: cpp

    void clearDuplicateCache();

Clear the duplicate advertisement filter cache.

Starting and Stopping
^^^^^^^^^^^^^^^^^^^^^

start
"""""

.. code-block:: cpp

    BTStatus start(uint32_t durationMs, bool continueExisting = false);

Start scanning for ``durationMs`` milliseconds (non-blocking). Set ``continueExisting`` to ``true`` to
append to existing results instead of clearing them.

startBlocking
"""""""""""""

.. code-block:: cpp

    BLEScanResults startBlocking(uint32_t durationMs);

Start scanning and block until complete. Returns the scan results directly.

stop
""""

.. code-block:: cpp

    BTStatus stop();

Stop an active scan.

isScanning
""""""""""

.. code-block:: cpp

    bool isScanning() const;

Callbacks
^^^^^^^^^

onResult
""""""""

.. code-block:: cpp

    void onResult(std::function<void(BLEAdvertisedDevice)> callback);

Called for each advertisement received during scanning.

onComplete
""""""""""

.. code-block:: cpp

    void onComplete(std::function<void(BLEScanResults &)> callback);

Called when a scan completes (duration expired).

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

Results Management
^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEScanResults getResults();
    void clearResults();
    void erase(const BTAddress &address);

Extended Scanning (BLE 5.0)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    struct ExtScanConfig {
        BLEPhy phy = BLEPhy::PHY_1M;
        uint16_t interval = 0;
        uint16_t window = 0;
    };

    BTStatus startExtended(uint32_t durationMs,
                           const ExtScanConfig *codedConfig = nullptr,
                           const ExtScanConfig *uncodedConfig = nullptr);
    BTStatus stopExtended();

Extended scanning supports separate configuration for coded PHY and uncoded PHY.

Periodic Sync (BLE 5.0)
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus createPeriodicSync(const BTAddress &addr, uint8_t sid,
                                uint16_t skipCount = 0, uint16_t timeoutMs = 10000);
    BTStatus cancelPeriodicSync();
    BTStatus terminatePeriodicSync(uint16_t syncHandle);

Synchronize to a periodic advertiser identified by address and advertising SID.

Periodic Sync Callbacks
"""""""""""""""""""""""

.. code-block:: cpp

    using PeriodicSyncHandler = std::function<void(uint16_t syncHandle, uint8_t sid,
                                                   const BTAddress &addr, BLEPhy phy,
                                                   uint16_t interval)>;
    using PeriodicReportHandler = std::function<void(uint16_t syncHandle, int8_t rssi,
                                                     int8_t txPower, const uint8_t *data,
                                                     size_t len)>;
    using PeriodicLostHandler = std::function<void(uint16_t syncHandle)>;

    void onPeriodicSync(PeriodicSyncHandler handler);
    void onPeriodicReport(PeriodicReportHandler handler);
    void onPeriodicLost(PeriodicLostHandler handler);

BLEAdvertisedDevice
*******************

Represents a single BLE device discovered during scanning. Shared handle with parsed advertisement data.

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEAdvertisedDevice();                 // default: null handle
    explicit operator bool() const;

Basic Information
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTAddress getAddress() const;
    BTAddress::Type getAddressType() const;
    String getName() const;
    int8_t getRSSI() const;
    int8_t getTXPower() const;
    uint16_t getAppearance() const;
    BLEAdvType getAdvType() const;

Manufacturer Data
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    const uint8_t *getManufacturerData(size_t *len) const;
    String getManufacturerDataString() const;
    uint16_t getManufacturerCompanyId() const;

Service UUIDs
^^^^^^^^^^^^^

.. code-block:: cpp

    size_t getServiceUUIDCount() const;
    BLEUUID getServiceUUID(size_t index = 0) const;
    bool haveServiceUUID() const;
    bool isAdvertisingService(const BLEUUID &uuid) const;

Service Data
^^^^^^^^^^^^

.. code-block:: cpp

    size_t getServiceDataCount() const;
    const uint8_t *getServiceData(size_t index, size_t *len) const;
    String getServiceDataString(size_t index = 0) const;
    BLEUUID getServiceDataUUID(size_t index = 0) const;
    bool haveServiceData() const;

Raw Payload
^^^^^^^^^^^

.. code-block:: cpp

    const uint8_t *getPayload() const;
    size_t getPayloadLength() const;

Presence Checks
^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool haveName() const;
    bool haveRSSI() const;
    bool haveTXPower() const;
    bool haveAppearance() const;
    bool haveManufacturerData() const;

Connectable / Scannable / Directed
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    bool isConnectable() const;
    bool isScannable() const;
    bool isDirected() const;
    bool isLegacyAdvertisement() const;

BLE 5.0 Extended Info
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEPhy getPrimaryPhy() const;
    BLEPhy getSecondaryPhy() const;
    uint8_t getAdvSID() const;
    uint16_t getPeriodicInterval() const;

Frame Type
^^^^^^^^^^

.. code-block:: cpp

    enum FrameType { Unknown, IBeacon, EddystoneUUID, EddystoneURL, EddystoneTLM };
    FrameType getFrameType() const;

toString
^^^^^^^^

.. code-block:: cpp

    String toString() const;

BLEScanResults
**************

Container for BLE scan results. Supports range-based ``for`` loops.

.. code-block:: cpp

    explicit operator bool() const;         // true if non-empty
    size_t getCount() const;
    BLEAdvertisedDevice getDevice(size_t index) const;
    void dump() const;

    const BLEAdvertisedDevice *begin() const;
    const BLEAdvertisedDevice *end() const;

Example:

.. code-block:: cpp

    BLEScanResults results = scan.startBlocking(5000);
    for (const auto &device : results) {
        Serial.printf("Found: %s RSSI=%d\n",
                       device.getName().c_str(),
                       device.getRSSI());
    }

Advertising
-----------

BLEAdvertising
**************

Obtained via ``BLE.getAdvertising()`` or ``server.getAdvertising()``. Supports legacy, extended, and
periodic advertising.

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEAdvertising();                      // default: null handle
    explicit operator bool() const;

Legacy Advertising
^^^^^^^^^^^^^^^^^^

Service UUIDs
"""""""""""""

.. code-block:: cpp

    void addServiceUUID(const BLEUUID &uuid);
    void removeServiceUUID(const BLEUUID &uuid);
    void clearServiceUUIDs();

Configuration
"""""""""""""

.. code-block:: cpp

    void setName(const String &name);
    void setScanResponse(bool enable);
    void setType(BLEAdvType type);
    void setInterval(uint16_t minMs, uint16_t maxMs);
    void setMinPreferred(uint16_t minPreferred);
    void setMaxPreferred(uint16_t maxPreferred);
    void setTxPower(bool include);
    void setAppearance(uint16_t appearance);
    void setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly);
    void reset();

.. note::

    When scan response is enabled (default after ``reset()``), the device name is placed in the
    **scan response** payload per CSS Part A §1.2, keeping the primary advertising data free for
    flags, service UUIDs, and appearance. When scan response is disabled via
    ``setScanResponse(false)``, the name is included in the primary advertising data instead.
    This behavior is consistent across both NimBLE and Bluedroid backends.

Custom Advertisement Data
"""""""""""""""""""""""""

.. code-block:: cpp

    void setAdvertisementData(const BLEAdvertisementData &data);
    void setScanResponseData(const BLEAdvertisementData &data);

Start / Stop
""""""""""""

.. code-block:: cpp

    BTStatus start(uint32_t durationMs = 0);
    BTStatus stop();
    bool isAdvertising() const;

Pass ``durationMs = 0`` for indefinite advertising.

Extended Advertising (BLE 5.0)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ExtAdvConfig
""""""""""""

.. code-block:: cpp

    struct ExtAdvConfig {
        uint8_t instance = 0;
        BLEAdvType type = BLEAdvType::ConnectableScannable;
        BLEPhy primaryPhy = BLEPhy::PHY_1M;
        BLEPhy secondaryPhy = BLEPhy::PHY_1M;
        int8_t txPower = 127;
        uint16_t intervalMin = 0x30;
        uint16_t intervalMax = 0x30;
        uint8_t channelMap = 0x07;
        uint8_t sid = 0;
        bool anonymous = false;
        bool includeTxPower = false;
        bool scanReqNotify = false;
    };

Methods
"""""""

.. code-block:: cpp

    BTStatus configureExtended(const ExtAdvConfig &config);
    BTStatus setExtAdvertisementData(uint8_t instance, const BLEAdvertisementData &data);
    BTStatus setExtScanResponseData(uint8_t instance, const BLEAdvertisementData &data);
    BTStatus setExtInstanceAddress(uint8_t instance, const BTAddress &addr);
    BTStatus startExtended(uint8_t instance, uint32_t durationMs = 0, uint8_t maxEvents = 0);
    BTStatus stopExtended(uint8_t instance);
    BTStatus removeExtended(uint8_t instance);
    BTStatus clearExtended();

Periodic Advertising (BLE 5.0)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

PeriodicAdvConfig
"""""""""""""""""

.. code-block:: cpp

    struct PeriodicAdvConfig {
        uint8_t instance = 0;
        uint16_t intervalMin = 0;
        uint16_t intervalMax = 0;
        bool includeTxPower = false;
    };

Methods
"""""""

.. code-block:: cpp

    BTStatus configurePeriodicAdv(const PeriodicAdvConfig &config);
    BTStatus setPeriodicAdvData(uint8_t instance, const BLEAdvertisementData &data);
    BTStatus startPeriodicAdv(uint8_t instance);
    BTStatus stopPeriodicAdv(uint8_t instance);

Event Handler
^^^^^^^^^^^^^

.. code-block:: cpp

    using CompleteHandler = std::function<void(uint8_t instance)>;
    void onComplete(CompleteHandler handler);

Called when an advertising duration expires or max events are reached.

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

BLEAdvertisementData
********************

Builder for raw BLE advertisement data payloads. Constructs AD structures per Bluetooth Core Spec
Vol 3, Part C, Section 11.

.. code-block:: cpp

    void setFlags(uint8_t flags);
    void setCompleteServices(const BLEUUID &uuid);
    void setPartialServices(const BLEUUID &uuid);
    void addServiceUUID(const BLEUUID &uuid);
    void setServiceData(const BLEUUID &uuid, const uint8_t *data, size_t len);
    void setManufacturerData(uint16_t companyId, const uint8_t *data, size_t len);
    void setName(const String &name, bool complete = true);
    void setShortName(const String &name);
    void setAppearance(uint16_t appearance);
    void setPreferredParams(uint16_t minInterval, uint16_t maxInterval);
    void setTxPower(int8_t txPower);
    void addRaw(const uint8_t *data, size_t len);
    void clear();

    const uint8_t *data() const;
    size_t length() const;

.. tip::

    Use ``BLEAdvertisementData`` when you need full control over the advertisement payload.
    For simple cases, use the convenience methods on ``BLEAdvertising`` directly.

Security
--------

BLESecurity
***********

Obtained via ``BLE.getSecurity()``. Configures security parameters, manages bonds, and provides
event handlers for the pairing flow.

.. code-block:: cpp

    BLESecurity sec = BLE.getSecurity();

Construction and Validity
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLESecurity();                         // default: null handle
    explicit operator bool() const;

IOCapability Enum
^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    enum IOCapability : uint8_t {
        DisplayOnly     = 0,
        DisplayYesNo    = 1,
        KeyboardOnly    = 2,
        NoInputNoOutput = 3,
        KeyboardDisplay = 4,
    };

IO Capability and Authentication
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

setIOCapability
"""""""""""""""

.. code-block:: cpp

    void setIOCapability(IOCapability cap);

Set the device's input/output capabilities for pairing.

setAuthenticationMode
"""""""""""""""""""""

.. code-block:: cpp

    void setAuthenticationMode(bool bonding, bool mitm, bool secureConnection);

Configure the authentication requirements:

* ``bonding`` -- persist keys for future reconnection
* ``mitm`` -- require Man-In-The-Middle protection (passkey or numeric comparison)
* ``secureConnection`` -- require LE Secure Connections (ECDH-based pairing)

Passkey Management
^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    void setPassKey(bool isStatic, uint32_t passkey = 0);
    void setStaticPassKey(uint32_t passkey);
    void setRandomPassKey();
    uint32_t getPassKey() const;
    static uint32_t generateRandomPassKey();
    void regenPassKeyOnConnect(bool enable);

``setStaticPassKey()`` sets a fixed 6-digit passkey. ``setRandomPassKey()`` generates a random one.
``regenPassKeyOnConnect(true)`` generates a new random passkey for each new pairing attempt.

Security Callbacks
^^^^^^^^^^^^^^^^^^

onPassKeyRequest
""""""""""""""""

.. code-block:: cpp

    using PassKeyRequestHandler = std::function<uint32_t(const BLEConnInfo &conn)>;
    void onPassKeyRequest(PassKeyRequestHandler handler);

Called when the stack needs a passkey from the user (keyboard-only devices). Return the passkey entered.

onPassKeyDisplay
""""""""""""""""

.. code-block:: cpp

    using PassKeyDisplayHandler = std::function<void(const BLEConnInfo &conn, uint32_t passKey)>;
    void onPassKeyDisplay(PassKeyDisplayHandler handler);

Called when the stack wants to display a passkey for the user to enter on the peer device.

onConfirmPassKey
""""""""""""""""

.. code-block:: cpp

    using ConfirmPassKeyHandler = std::function<bool(const BLEConnInfo &conn, uint32_t passKey)>;
    void onConfirmPassKey(ConfirmPassKeyHandler handler);

Called during numeric comparison pairing. Display the passkey and return ``true`` if the user confirms
it matches.

onSecurityRequest
"""""""""""""""""

.. code-block:: cpp

    using SecurityRequestHandler = std::function<bool(const BLEConnInfo &conn)>;
    void onSecurityRequest(SecurityRequestHandler handler);

Called when a peer initiates a security request. Return ``true`` to accept.

onAuthorization
"""""""""""""""

.. code-block:: cpp

    using AuthorizationHandler = std::function<bool(const BLEConnInfo &conn, uint16_t attrHandle, bool isRead)>;
    void onAuthorization(AuthorizationHandler handler);

Called when an attribute access requires authorization. Return ``true`` to allow.

onAuthenticationComplete
""""""""""""""""""""""""

.. code-block:: cpp

    using AuthCompleteHandler = std::function<void(const BLEConnInfo &conn, bool success)>;
    void onAuthenticationComplete(AuthCompleteHandler handler);

Called when the authentication/pairing process completes.

resetCallbacks
""""""""""""""

.. code-block:: cpp

    void resetCallbacks();

Clear all registered callbacks.

Key Distribution
^^^^^^^^^^^^^^^^

KeyDist Enum
""""""""""""

.. code-block:: cpp

    enum class KeyDist : uint8_t {
        EncKey  = 0x01,   // Encryption key (LTK)
        IdKey   = 0x02,   // Identity key (IRK)
        SignKey = 0x04,   // Signing key (CSRK)
        LinkKey = 0x08,   // Link key (BR/EDR)
    };

Combine with bitwise OR: ``KeyDist::EncKey | KeyDist::IdKey``.

.. code-block:: cpp

    void setInitiatorKeys(KeyDist keys);
    void setResponderKeys(KeyDist keys);
    void setKeySize(uint8_t size);

Force Authentication
^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    void setForceAuthentication(bool force);
    bool getForceAuthentication() const;

When enabled, all GATT attribute access requires an authenticated connection.

Bond Management
^^^^^^^^^^^^^^^

.. code-block:: cpp

    std::vector<BTAddress> getBondedDevices() const;
    BTStatus deleteBond(const BTAddress &address);
    BTStatus deleteAllBonds();

Bond Store Overflow
"""""""""""""""""""

.. code-block:: cpp

    using BondStoreOverflowHandler = std::function<void(const BTAddress &oldestBond)>;
    void onBondStoreOverflow(BondStoreOverflowHandler handler);

Called when the bond store is full and a new bond needs to be stored.

Security Flow Methods
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus startSecurity(uint16_t connHandle);
    bool waitForAuthenticationComplete(uint32_t timeoutMs = 10000);
    void resetSecurity();

``startSecurity()`` initiates pairing on a specific connection. ``waitForAuthenticationComplete()`` blocks
until pairing completes or times out.

Shared Types
------------

BTAddress
*********

Unified Bluetooth address type for both BLE and BT Classic.

Internally stores 6 bytes in LSB-first (Bluetooth standard) order. Displayed as
``"AA:BB:CC:DD:EE:FF"`` (MSB-first, human-readable).

Type Enum
^^^^^^^^^

.. code-block:: cpp

    enum Type : uint8_t {
        Public   = 0,
        Random   = 1,
        PublicID = 2,
        RandomID = 3,
    };

Constructors
^^^^^^^^^^^^

.. code-block:: cpp

    BTAddress();                                        // zero address
    BTAddress(const uint8_t addr[6], Type type = Public);  // from raw bytes (LSB-first)
    BTAddress(const char *str);                         // from "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF"
    BTAddress(const String &str);

Methods
^^^^^^^

.. code-block:: cpp

    bool operator==(const BTAddress &other) const;
    bool operator!=(const BTAddress &other) const;
    bool operator<(const BTAddress &other) const;
    explicit operator bool() const;         // true if non-zero
    const uint8_t *data() const;            // internal 6-byte array (LSB-first)
    Type type() const;
    String toString(bool uppercase = false) const;
    bool isZero() const;

BTStatus
********

Unified error/status type. 1 byte. ``explicit operator bool()`` returns ``true`` on success.

Status Codes
^^^^^^^^^^^^

.. code-block:: cpp

    enum Code : int8_t {
        OK               =  0,
        Fail             = -1,
        NotInitialized   = -2,
        InvalidParam     = -3,
        InvalidState     = -4,
        NoMemory         = -5,
        Timeout          = -6,
        NotConnected     = -7,
        AlreadyConnected = -8,
        NotFound         = -9,
        AuthFailed       = -10,
        PermissionDenied = -11,
        Busy             = -12,
        NotSupported     = -13,
    };

Methods
^^^^^^^

.. code-block:: cpp

    BTStatus();                     // defaults to OK
    BTStatus(Code c);
    explicit operator bool() const; // true when OK
    bool operator!() const;
    bool operator==(const BTStatus &o) const;
    bool operator!=(const BTStatus &o) const;
    Code code() const;
    const char *toString() const;

Usage:

.. code-block:: cpp

    BTStatus s = client.connect(addr);
    if (!s) {
        Serial.printf("Failed: %s\n", s.toString());
    }
    if (s == BTStatus::Timeout) {
        Serial.println("Timed out");
    }

BLEUUID
*******

Stack-agnostic UUID type supporting 16-bit, 32-bit, and 128-bit UUIDs.

Stores internally as ``uint8_t[16]`` in big-endian (display) order. A ``bitSize()`` of 0 indicates
invalid/unset.

Constructors
^^^^^^^^^^^^

.. code-block:: cpp

    BLEUUID();                                          // invalid UUID
    BLEUUID(const char *uuid);                          // "180D", "0000180D", or full 128-bit
    BLEUUID(uint16_t uuid16);
    BLEUUID(uint32_t uuid32);
    BLEUUID(const uint8_t uuid128[16]);                 // big-endian
    BLEUUID(const uint8_t *data, size_t len, bool reverse);  // explicit byte order

String parsing accepts:

* ``"180D"`` -- 16-bit
* ``"0000180D"`` -- 32-bit
* ``"0000180d-0000-1000-8000-00805f9b34fb"`` -- 128-bit with dashes
* ``"0000180d00001000800000805f9b34fb"`` -- 128-bit without dashes

Methods
^^^^^^^

.. code-block:: cpp

    bool operator==(const BLEUUID &other) const;
    bool operator!=(const BLEUUID &other) const;
    bool operator<(const BLEUUID &other) const;
    String toString() const;
    uint8_t bitSize() const;         // 16, 32, 128, or 0 (invalid)
    BLEUUID to128() const;           // expand to 128-bit using Bluetooth Base UUID
    bool isValid() const;
    explicit operator bool() const;  // equivalent to isValid()
    const uint8_t *data() const;     // internal 128-bit representation
    uint16_t toUint16() const;       // valid only when bitSize() == 16
    uint32_t toUint32() const;       // valid only when bitSize() == 32

BLEProperty
***********

Characteristic properties -- Bluetooth Core Spec Vol 3, Part G, 3.3.1.1. Combine with bitwise OR.

.. code-block:: cpp

    enum class BLEProperty : uint8_t {
        Broadcast     = 0x01,
        Read          = 0x02,
        WriteNR       = 0x04,  // Write Without Response
        Write         = 0x08,
        Notify        = 0x10,
        Indicate      = 0x20,
        SignedWrite   = 0x40,
        ExtendedProps = 0x80,
    };

Example:

.. code-block:: cpp

    BLEProperty props = BLEProperty::Read | BLEProperty::Write | BLEProperty::Notify;

BLEPermission
*************

Attribute access permissions -- Bluetooth Core Spec Vol 3, Part F, 3.2.5. Combine with bitwise OR.

.. code-block:: cpp

    enum class BLEPermission : uint16_t {
        Read              = 0x0001,
        ReadEncrypted     = 0x0002,
        ReadAuthenticated = 0x0004,
        ReadAuthorized    = 0x0008,
        Write             = 0x0010,
        WriteEncrypted    = 0x0020,
        WriteAuthenticated= 0x0040,
        WriteAuthorized   = 0x0080,
    };

Example:

.. code-block:: cpp

    BLEPermission perms = BLEPermission::ReadEncrypted | BLEPermission::WriteAuthenticated;

BLEPermissions Presets
^^^^^^^^^^^^^^^^^^^^^^

Convenience constants in the ``BLEPermissions::`` namespace for common security configurations:

.. code-block:: cpp

    // Open — no security requirement
    BLEPermissions::OpenRead
    BLEPermissions::OpenWrite
    BLEPermissions::OpenReadWrite

    // Encrypted — pairing required, no MITM
    BLEPermissions::EncryptedRead
    BLEPermissions::EncryptedWrite
    BLEPermissions::EncryptedReadWrite

    // Authenticated — MITM-protected pairing (implies encryption)
    BLEPermissions::AuthenticatedRead
    BLEPermissions::AuthenticatedWrite
    BLEPermissions::AuthenticatedReadWrite

    // Authorized — application-level authorization callback
    BLEPermissions::AuthorizedRead
    BLEPermissions::AuthorizedWrite
    BLEPermissions::AuthorizedReadWrite

Use these with ``createCharacteristic()`` for readable, self-documenting permission declarations.

BLEConnInfo
***********

Stack-agnostic connection descriptor. Pure value type with inline storage (no heap allocation).

.. code-block:: cpp

    BLEConnInfo();
    explicit operator bool() const;         // true if valid

    uint16_t getHandle() const;             // connection handle
    BTAddress getAddress() const;           // OTA peer address
    BTAddress getIdAddress() const;         // identity address (after IRK resolution)
    uint16_t getMTU() const;                // negotiated ATT MTU
    bool isEncrypted() const;
    bool isAuthenticated() const;
    bool isBonded() const;
    uint8_t getSecurityKeySize() const;     // 7-16 bytes
    uint16_t getInterval() const;           // in 1.25 ms units
    uint16_t getLatency() const;            // peripheral latency
    uint16_t getSupervisionTimeout() const; // in 10 ms units
    BLEPhy getTxPhy() const;
    BLEPhy getRxPhy() const;
    bool isCentral() const;
    bool isPeripheral() const;
    int8_t getRSSI() const;                 // -127 to +20 dBm

BLEConnParams
*************

Connection parameter structure.

.. code-block:: cpp

    struct BLEConnParams {
        uint16_t minInterval;  // 6..3200 (7.5ms..4s in 1.25ms units)
        uint16_t maxInterval;  // 6..3200
        uint16_t latency;      // 0..499
        uint16_t timeout;      // 10..3200 (100ms..32s in 10ms units)

        bool isValid() const;  // validates against Bluetooth Core Spec
    };

BLEAdvType
**********

.. code-block:: cpp

    enum class BLEAdvType : uint8_t {
        Connectable,
        ConnectableScannable,
        ConnectableDirected,
        ScannableUndirected,
        NonConnectable,
        DirectedHighDuty,
        DirectedLowDuty,
    };

BLEPhy
******

.. code-block:: cpp

    enum class BLEPhy : uint8_t {
        PHY_1M    = 1,
        PHY_2M    = 2,
        PHY_Coded = 3,
    };

Specialty Features
------------------

BLEBeacon (iBeacon)
*******************

Apple iBeacon advertisement builder/parser.

.. code-block:: cpp

    #include <BLE.h>

    BLEBeacon beacon;
    beacon.setProximityUUID(BLEUUID("FDA50693-A4E2-4FB1-AFCF-C6EB07647825"));
    beacon.setMajor(1);
    beacon.setMinor(1);
    beacon.setSignalPower(-59);
    BLEAdvertisementData advData = beacon.getAdvertisementData();

Methods
^^^^^^^

.. code-block:: cpp

    BLEBeacon();

    BLEUUID getProximityUUID() const;
    void setProximityUUID(const BLEUUID &uuid);

    uint16_t getMajor() const;
    void setMajor(uint16_t major);

    uint16_t getMinor() const;
    void setMinor(uint16_t minor);

    uint16_t getManufacturerId() const;
    void setManufacturerId(uint16_t id);

    int8_t getSignalPower() const;
    void setSignalPower(int8_t power);

    BLEAdvertisementData getAdvertisementData() const;
    void setFromPayload(const uint8_t *payload, size_t len);

BLEEddystone (URL + TLM)
*************************

Google Eddystone frame builders/parsers.

BLEEddystoneURL
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    #include <BLE.h>

    BLEEddystoneURL url;
    url.setURL("https://espressif.com");
    url.setTxPower(-20);
    BLEAdvertisementData advData = url.getAdvertisementData();

Methods:

.. code-block:: cpp

    BLEEddystoneURL();
    void setURL(const String &url);
    String getURL() const;
    void setTxPower(int8_t txPower);
    int8_t getTxPower() const;
    BLEAdvertisementData getAdvertisementData() const;
    void setFromServiceData(const uint8_t *data, size_t len);
    static BLEUUID serviceUUID();

BLEEddystoneTLM
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEEddystoneTLM tlm;
    tlm.setBatteryVoltage(3300);
    tlm.setTemperature(25.5f);
    tlm.setAdvertisingCount(100);
    tlm.setUptime(36000);
    BLEAdvertisementData advData = tlm.getAdvertisementData();

Methods:

.. code-block:: cpp

    BLEEddystoneTLM();
    void setBatteryVoltage(uint16_t millivolts);
    uint16_t getBatteryVoltage() const;
    void setTemperature(float celsius);
    float getTemperature() const;
    void setAdvertisingCount(uint32_t count);
    uint32_t getAdvertisingCount() const;
    void setUptime(uint32_t deciseconds);
    uint32_t getUptime() const;
    BLEAdvertisementData getAdvertisementData() const;
    void setFromServiceData(const uint8_t *data, size_t len);
    String toString() const;
    static BLEUUID serviceUUID();

BLEHIDDevice (HID-over-GATT)
*****************************

Helper for creating HID-over-GATT (HOGP) devices. Manages the standard GATT services:

* Device Information Service (0x180A)
* HID Service (0x1812)
* Battery Service (0x180F) — automatically declared as an Included Service of the HID Service (HIDS 1.0 §3)

The constructor also:

* Adds an **External Report Reference** descriptor (0x2907) on the Report Map characteristic,
  referencing the Battery Level UUID (0x2A19), as required by HIDS 1.0 §3.6.
* Adds the **HID Service UUID** (0x1812) to the advertising data automatically, so user code
  does not need to call ``adv.addServiceUUID()`` for HID discovery.

.. code-block:: cpp

    #include <BLE.h>

    BLEServer server = BLE.createServer();
    BLEHIDDevice hid(server);
    hid.manufacturer("Espressif");
    hid.pnp(0x02, 0x1234, 0x5678, 0x0100);
    hid.hidInfo(0x00, 0x01);
    hid.reportMap(myDescriptor, sizeof(myDescriptor));
    BLECharacteristic input = hid.inputReport(1);
    server.start();

    BLEAdvertising adv = BLE.getAdvertising();
    adv.setAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    adv.start();

Methods
^^^^^^^

.. code-block:: cpp

    explicit BLEHIDDevice(BLEServer server);

    void reportMap(const uint8_t *map, uint16_t size);

    BLEService deviceInfoService();
    BLEService hidService();
    BLEService batteryService();

    BLECharacteristic manufacturer();
    void manufacturer(const String &name);

    void pnp(uint8_t sig, uint16_t vendorId, uint16_t productId, uint16_t version);
    void hidInfo(uint8_t country, uint8_t flags);

    void setBatteryLevel(uint8_t level);

    BLECharacteristic hidControl();
    BLECharacteristic protocolMode();

    BLECharacteristic inputReport(uint8_t reportId);
    BLECharacteristic outputReport(uint8_t reportId);
    BLECharacteristic featureReport(uint8_t reportId);

    BLECharacteristic bootInput();
    BLECharacteristic bootOutput();

``inputReport()``, ``outputReport()``, and ``featureReport()`` each create a new Report
characteristic (UUID 0x2A4D) with a Report Reference descriptor. Multiple reports with the
same report ID but different types (input vs output) are supported.

``bootInput()`` and ``bootOutput()`` use lazy creation — calling them multiple times returns
the same cached characteristic.

HID Appearance Constants
^^^^^^^^^^^^^^^^^^^^^^^^

Defined in ``HIDTypes.h`` for use with ``adv.setAppearance()``:

.. code-block:: cpp

    #define BLE_APPEARANCE_HID_GENERIC         0x03C0
    #define BLE_APPEARANCE_HID_KEYBOARD        0x03C1
    #define BLE_APPEARANCE_HID_MOUSE           0x03C2
    #define BLE_APPEARANCE_HID_JOYSTICK        0x03C3
    #define BLE_APPEARANCE_HID_GAMEPAD         0x03C4
    #define BLE_APPEARANCE_HID_DIGITIZER       0x03C5
    #define BLE_APPEARANCE_HID_CARD_READER     0x03C6
    #define BLE_APPEARANCE_HID_DIGITAL_PEN     0x03C7
    #define BLE_APPEARANCE_HID_BARCODE_SCANNER 0x03C8

L2CAP CoC (Connection-Oriented Channels)
*****************************************

L2CAP CoC channels provide a higher-throughput alternative to GATT for bulk data transfer,
bypassing the GATT attribute protocol overhead. Requires NimBLE with ``BLE_L2CAP_SUPPORTED``
(set ``CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM > 0`` in ``sdkconfig``).

Factory Methods (BLEClass)
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BLEL2CAPServer createL2CAPServer(uint16_t psm, uint16_t mtu = 512);
    BLEL2CAPChannel connectL2CAP(uint16_t connHandle, uint16_t psm, uint16_t mtu = 512);

``createL2CAPServer()`` starts listening for incoming L2CAP CoC connections on the given PSM.
``connectL2CAP()`` opens an outgoing channel to an already-connected peer.

BLEL2CAPChannel
^^^^^^^^^^^^^^^

Represents a single bidirectional L2CAP CoC channel.

Construction and Validity
"""""""""""""""""""""""""

.. code-block:: cpp

    BLEL2CAPChannel();
    explicit operator bool() const;

Data Transfer
"""""""""""""

.. code-block:: cpp

    BTStatus write(const uint8_t *data, size_t len);
    BTStatus disconnect();
    bool isConnected() const;
    uint16_t getPSM() const;
    uint16_t getMTU() const;
    uint16_t getConnHandle() const;

Callbacks
"""""""""

.. code-block:: cpp

    using DataHandler = std::function<void(BLEL2CAPChannel channel, const uint8_t *data, size_t len)>;
    using DisconnectHandler = std::function<void(BLEL2CAPChannel channel)>;
    BTStatus onData(DataHandler handler);
    BTStatus onDisconnect(DisconnectHandler handler);
    BTStatus resetCallbacks();

.. note::

    L2CAP callback registration methods return ``BTStatus``, unlike most other BLE classes where
    they return ``void``.

BLEL2CAPServer
^^^^^^^^^^^^^^

Listens for incoming L2CAP CoC connections on a PSM.

Construction and Validity
"""""""""""""""""""""""""

.. code-block:: cpp

    BLEL2CAPServer();
    explicit operator bool() const;

Callbacks
"""""""""

.. code-block:: cpp

    using AcceptHandler = std::function<void(BLEL2CAPChannel channel)>;
    using DataHandler = std::function<void(BLEL2CAPChannel channel, const uint8_t *data, size_t len)>;
    using DisconnectHandler = std::function<void(BLEL2CAPChannel channel)>;
    BTStatus onAccept(AcceptHandler handler);
    BTStatus onData(DataHandler handler);
    BTStatus onDisconnect(DisconnectHandler handler);
    BTStatus resetCallbacks();

Per-channel handlers registered via ``BLEL2CAPChannel::onData()`` take precedence over the
server-level default.

Accessors
"""""""""

.. code-block:: cpp

    uint16_t getPSM() const;
    uint16_t getMTU() const;

BLEStream (Nordic UART Service)
********************************

``BLEStream`` wraps BLE GATT operations behind the familiar Arduino ``Stream`` API using the
Nordic UART Service (NUS) UUID conventions. It is interoperable with standard BLE serial apps
(nRF Connect, Serial Bluetooth Terminal, etc.).

``BLEStream`` is **non-copyable** and owns its server/client resources internally.

Server Mode
^^^^^^^^^^^

.. code-block:: cpp

    BLEStream bleStream;
    bleStream.begin("MyDevice");
    if (bleStream.connected()) {
        bleStream.println("Hello World!");
        if (bleStream.available()) {
            String s = bleStream.readStringUntil('\n');
        }
    }

Client Mode
^^^^^^^^^^^

.. code-block:: cpp

    BLEStream bleStream;
    bleStream.beginClient(remoteAddr);
    bleStream.println("Hello Server!");

Lifecycle
^^^^^^^^^

.. code-block:: cpp

    BLEStream(size_t rxBufferSize = 256);
    BTStatus begin(const String &deviceName = "BLE_Stream");
    BTStatus beginClient(const BTAddress &address, uint32_t timeoutMs = 5000);
    void end();
    bool connected() const;

Arduino Stream Interface
^^^^^^^^^^^^^^^^^^^^^^^^

``BLEStream`` implements the standard Arduino ``Stream`` methods:

.. code-block:: cpp

    int available() override;
    int read() override;
    int peek() override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    void flush() override;

Callbacks
^^^^^^^^^

.. code-block:: cpp

    using ConnectHandler = std::function<void(const BLEConnInfo &connInfo)>;
    using DisconnectHandler = std::function<void(const BLEConnInfo &connInfo, uint8_t reason)>;
    void onConnect(ConnectHandler handler);
    void onDisconnect(DisconnectHandler handler);
    void resetCallbacks();

NUS UUIDs
^^^^^^^^^

.. code-block:: cpp

    static BLEUUID nusServiceUUID();
    static BLEUUID nusRxCharUUID();
    static BLEUUID nusTxCharUUID();

Migration from v3.x
--------------------

See the `Migration Guide <https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/MIGRATION.md>`_
for a comprehensive old-vs-new API mapping, before/after code examples, and ArduinoBLE porting guide.
