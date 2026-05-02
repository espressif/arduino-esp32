#################
Bluetooth Classic
#################

About
-----

The BluetoothSerial library provides a UART-like interface over Bluetooth Classic Serial Port Profile (SPP) for the ESP32. It extends the Arduino ``Stream`` class, making it a drop-in replacement for ``Serial`` in many applications.

.. note::

    Bluetooth Classic is only supported on the original ESP32. It is not available on ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2, or ESP32-P4.

.. warning::

    The ``BluetoothSerial`` class is annotated with ``[[deprecated]]`` and will not be supported by default in version 4.0.0.

Key features:

* Stream-compatible API (``read()``, ``write()``, ``available()``, ``print()``, ``println()``)
* Initiator (master) and acceptor (slave) modes
* Secure Simple Pairing (SSP) with ``std::function`` callbacks
* Legacy PIN pairing via ``setPin()``
* Device discovery (synchronous and asynchronous)
* Bond management (list and delete bonded devices)
* RAII resource management with ``std::unique_ptr<Impl>`` (PIMPL pattern)
* ``BTStatus`` return values for error handling on most operations

Architecture
------------

``BluetoothSerial`` follows the PIMPL (Pointer to Implementation) pattern. The public header declares only the API surface while all Bluedroid-specific state lives behind a private ``struct Impl`` held by ``std::unique_ptr<Impl>``.

Ownership semantics
*******************

* The class is **not copyable** -- copy constructor and copy-assignment are ``= delete``.
* It **is movable** -- move constructor and move-assignment are ``= default``, transferring sole ownership of the ``Impl`` via ``std::unique_ptr``.

RAII lifecycle
**************

FreeRTOS resources (queues, semaphores, task handles) are allocated inside ``begin()`` and released in ``end()`` or the destructor. Because allocation can fail, ``begin()`` returns ``BTStatus`` -- check for ``BTStatus::NoMemory`` or ``BTStatus::Timeout`` to detect resource-creation failures.

Thread safety
*************

SPP callbacks (data received, authentication events) execute on the Bluetooth controller task, while user code runs on the Arduino ``loop()`` task. Shared state is protected by FreeRTOS primitives inside the ``Impl`` struct. Avoid blocking the BT task in callbacks.

``explicit operator bool``
**************************

``BluetoothSerial`` provides ``explicit operator bool() const``. It returns ``true`` when the Bluetooth stack has been initialized successfully (i.e., ``begin()`` returned ``BTStatus::OK``). Because the conversion is ``explicit``, implicit boolean conversions are not allowed; use ``if (static_cast<bool>(SerialBT))`` or simply ``if (SerialBT.connected())``.

Design notes
------------

* **Deprecated** -- The class is annotated with ``[[deprecated("BluetoothSerial won't be supported in version 4.0.0 by default")]]``. Compiler warnings are expected; this is intentional.
* **ESP32 only** -- Bluetooth Classic (BR/EDR) hardware is present only on the original ESP32 SoC.
* **SPP profile** -- Communication uses the Serial Port Profile, which provides a virtual serial link over L2CAP/RFCOMM.
* **Security** -- Both server (acceptor) and client (initiator) modes use ``ESP_SPP_SEC_AUTHENTICATE``. Incoming and outgoing connections require authentication.
* **Fallible resource creation** -- ``begin()`` may return ``BTStatus::NoMemory`` (FreeRTOS allocation failure) or ``BTStatus::Timeout`` (controller did not respond in time). Always check the return value.

Getting Started
---------------

.. code-block:: cpp

    #include <BluetoothSerial.h>

    BluetoothSerial SerialBT;

    void setup() {
        Serial.begin(115200);
        SerialBT.begin("ESP32-BT");
        Serial.println("Bluetooth started! Pair and connect from your device.");
    }

    void loop() {
        if (Serial.available()) {
            SerialBT.write(Serial.read());
        }
        if (SerialBT.available()) {
            Serial.write(SerialBT.read());
        }
    }

Build requirements
******************

BluetoothSerial is compiled only when Bluetooth is enabled in sdkconfig: ``SOC_BT_SUPPORTED``, ``CONFIG_BT_ENABLED``, and ``CONFIG_BLUEDROID_ENABLED`` must be set. SPP must be enabled (``CONFIG_BT_SPP_ENABLED``) for serial bridge use cases.

API Reference
-------------

Lifecycle
*********

begin
^^^^^

Initialize Bluetooth and start SPP.

.. code-block:: cpp

    BTStatus SerialBT.begin(const String &localName = "ESP32", bool isInitiator = false);

* ``localName``: Device name visible during discovery and to remote peers.
* ``isInitiator``: If ``true``, operates in master (initiator) mode and may connect outbound to other devices; if ``false``, operates as acceptor (slave) and waits for incoming connections.

Returns ``BTStatus::OK`` on success, ``BTStatus::NoMemory`` or ``BTStatus::Timeout`` on failure.

end
^^^

Stop Bluetooth and release resources.

.. code-block:: cpp

    void SerialBT.end(bool releaseMemory = false);

* ``releaseMemory``: When ``true``, releases additional internal allocations where applicable.

Stream interface
****************

``BluetoothSerial`` extends ``Stream``, providing:

.. code-block:: cpp

    int SerialBT.available();
    int SerialBT.read();
    size_t SerialBT.write(uint8_t c);
    size_t SerialBT.write(const uint8_t *buf, size_t len);
    int SerialBT.peek();
    void SerialBT.flush();

All standard ``print()`` and ``println()`` methods from ``Print``/``Stream`` are available.

.. note::

    ``flush()`` waits until the TX queue drains but does NOT wait for data to be acknowledged
    on the wire. Bytes may still be in-flight when ``flush()`` returns.

Connection
**********

connect
^^^^^^^

Connect to a remote device. Only used in initiator (master) mode after ``begin(..., true)``.

.. code-block:: cpp

    BTStatus SerialBT.connect(const String &remoteName, uint32_t timeoutMs = 10000);
    BTStatus SerialBT.connect(const BTAddress &address, uint8_t channel = 0);

* ``remoteName``: Friendly name inquiry; ``timeoutMs`` bounds how long to wait for a match.
* ``address``: Bluetooth address of the peer; ``channel`` selects the RFCOMM channel when non-zero.

Returns ``BTStatus`` (for example ``BTStatus::OK``, ``BTStatus::Timeout``, ``BTStatus::NotFound``).

disconnect
^^^^^^^^^^

.. code-block:: cpp

    BTStatus SerialBT.disconnect();

Disconnects the active SPP session. Returns ``BTStatus::OK`` on success, ``BTStatus::Timeout`` if the
disconnection did not complete within the internal timeout.

connected
^^^^^^^^^

.. code-block:: cpp

    bool SerialBT.connected() const;

Returns ``true`` when an SPP link is established.

hasClient
^^^^^^^^^

.. code-block:: cpp

    bool SerialBT.hasClient() const;

Equivalent to ``connected()`` in the current implementation; provided for readability in server-style sketches.

Device discovery
****************

discover (synchronous)
^^^^^^^^^^^^^^^^^^^^^^

Performs a blocking inquiry and returns accumulated results when discovery completes or times out.

.. code-block:: cpp

    std::vector<BluetoothSerial::DiscoveryResult> SerialBT.discover(uint32_t timeoutMs = 10000);

Each ``DiscoveryResult`` contains:

.. code-block:: cpp

    struct DiscoveryResult {
        BTAddress address;
        String name;
        uint32_t cod = 0;    // Class of Device
        int8_t rssi = -128;
    };

discoverAsync
^^^^^^^^^^^^^

Starts an asynchronous inquiry. Each found device is reported through the callback until discovery ends.

.. code-block:: cpp

    BTStatus SerialBT.discoverAsync(
        std::function<void(const DiscoveryResult &)> callback,
        uint32_t timeoutMs = 10000
    );

The ``DiscoveryResult`` type is ``BluetoothSerial::DiscoveryResult``.

discoverStop
^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus SerialBT.discoverStop();

Cancels an in-progress discovery and clears the async callback.

Security
********

enableSSP / disableSSP
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    void SerialBT.enableSSP(bool input = false, bool output = false);
    void SerialBT.disableSSP();

``enableSSP`` turns on Secure Simple Pairing. The ``input`` and ``output`` flags map to IO capability hints used during pairing (whether the device can display or accept user input).

setPin
^^^^^^

Set a legacy PIN code for pairing (up to 16 characters; longer strings are truncated internally).

.. code-block:: cpp

    void SerialBT.setPin(const char *pin);

Bond management
***************

.. code-block:: cpp

    std::vector<BTAddress> SerialBT.getBondedDevices();
    BTStatus SerialBT.deleteBond(const BTAddress &address);
    BTStatus SerialBT.deleteAllBonds();

``getBondedDevices()`` returns all addresses currently stored in the controller bond list. The number of bonded devices is ``getBondedDevices().size()``.

Callbacks
*********

onData
^^^^^^

.. code-block:: cpp

    BTStatus SerialBT.onData(std::function<void(const uint8_t *, size_t)> callback);

Registers a callback invoked on the BT task whenever SPP data arrives. Returns ``BTStatus::OK`` on success.

onConfirmRequest
^^^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus SerialBT.onConfirmRequest(std::function<bool(uint32_t)> callback);

Registers a callback for SSP numeric-comparison pairing. The ``uint32_t`` argument is the numeric value to display to the user. The callback must return ``true`` to accept the pairing or ``false`` to reject it. Returns ``BTStatus::OK`` on success.

onAuthComplete
^^^^^^^^^^^^^^

.. code-block:: cpp

    BTStatus SerialBT.onAuthComplete(std::function<void(bool)> callback);

Registers a callback invoked when authentication finishes. The ``bool`` argument is ``true`` on success. Returns ``BTStatus::OK`` on success.

Registration returns ``BTStatus::OK`` when the callback was stored, or an error such as ``BTStatus::InvalidState`` if Bluetooth is not in a valid state.

Info and state
**************

getAddress
^^^^^^^^^^

.. code-block:: cpp

    BTAddress SerialBT.getAddress() const;

Returns the local public Bluetooth address, or a zero address if not initialized.

operator bool
^^^^^^^^^^^^^

.. code-block:: cpp

    explicit operator bool() const;

Returns ``true`` when the Bluetooth stack has been initialized successfully (``begin()`` returned ``BTStatus::OK``). Because the conversion is ``explicit``, implicit boolean conversions are not allowed; use ``static_cast<bool>(SerialBT)`` or test after ``begin()``.

Shared types
------------

BTAddress
*********

Unified Bluetooth address shared between BLE and Bluetooth Classic.

.. code-block:: cpp

    BTAddress addr("aa:bb:cc:dd:ee:ff");
    String str = addr.toString();

Constructors also accept a raw 6-byte array (LSB-first) and Arduino ``String``. See ``BTAddress.h`` in ``cores/esp32`` for full details.

BTStatus
********

Unified status type for Bluetooth operations with ``explicit operator bool()`` (success when ``OK``).

.. code-block:: cpp

    BTStatus status = SerialBT.begin("ESP32");
    if (!status) {
        Serial.printf("Error: %s\n", status.toString());
    }

Common codes include ``BTStatus::OK``, ``BTStatus::Fail``, ``BTStatus::NotInitialized``, ``BTStatus::Timeout``, ``BTStatus::NoMemory``, ``BTStatus::NotConnected``, ``BTStatus::AuthFailed``, and others defined in ``BTStatus.h``.
