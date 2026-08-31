######
Matter
######

About
-----

The Matter library provides support for creating Matter-compatible devices including:

* Support for Wi-Fi and Thread connectivity
* Matter commissioning via QR code or manual pairing code
* Multiple endpoint types for various device categories
* Semantic tags (Descriptor cluster TagList) via ``MatterEndPoint::setTagList()`` to disambiguate sibling endpoints
* Event monitoring and callback support
* Integration with Home Assistant, Apple HomeKit, Amazon Alexa, and Google Home
* Smart home ecosystem compatibility

The Matter library is built on top of `ESP Matter SDK <https://github.com/espressif/esp-matter>`_ and provides a high-level Arduino-style interface for creating Matter devices.

Building and Flashing Matter Examples
--------------------------------------

Before uploading any Matter example sketch, it is necessary to configure the Arduino IDE with the following settings:

1. **Partition Scheme**: Select **"Huge APP (3 MB No OTA / 1 MB SPIFFS)"** from **Tools > Partition Scheme** menu.

  .. figure:: ../../_static/matter_partition_scheme.png
      :align: center
      :alt: "Partition Scheme: Huge APP (3 MB No OTA / 1 MB SPIFFS)" Arduino IDE menu option
      :figclass: align-center

2. **Erase Flash**: Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.

  .. figure:: ../../_static/matter_erase_flash.png
      :align: center
      :alt: "Erase All Flash Before Sketch Upload: Enabled" Arduino IDE menu option
      :figclass: align-center

These settings are required for the following reasons:

* **Partition Scheme**: Matter firmware requires a large application partition (3 MB) to accommodate the Matter stack and application code.
* **Erase Flash**: Erasing flash is necessary to remove any leftover Wi-Fi or Matter configuration from the NVS (Non-Volatile Storage) partition. Without erasing, previous network credentials, Matter fabric information, or device commissioning data may interfere with the new firmware, causing commissioning failures or connectivity issues.

Matter Protocol Overview
************************

Matter (formerly Project CHIP - Connected Home over IP) is an open-source connectivity standard for smart home devices. It enables seamless communication between devices from different manufacturers, allowing them to work together within a single ecosystem.

**Key Features:**

* **Multi-Protocol Support**: Works over Wi-Fi, Thread, and Ethernet
* **Interoperability**: Devices from different manufacturers work together
* **Security**: Built-in security features including encryption and authentication
* **Local Control**: Devices can communicate locally without requiring cloud connectivity
* **Simple Setup**: Easy commissioning via QR code or pairing code

Matter Network Topology
***********************

.. code-block:: text

    ┌─────────────────┐       ┌──────────────────┐       ┌─────────────────┐
    │   Matter Hub    │◄─────►│   Wi-Fi Router   │◄─────►│  ESP Thread     │
    │  (HomePod etc)  │       │                  │       │  Border Router  │
    └─────────────────┘       └──────────────────┘       └─────────────────┘
            │                          │                         │
            │                          ▼                         │
            │                 ┌─────────────────┐                │
            │                 │  Matter Device  │                │
            │                 │  (via Wi-Fi)    │                │
            │                 └─────────────────┘                │
            │                                                    │
            ▼                                                    ▼
    ┌─────────────────┐                                 ┌─────────────────┐
    │  Matter Device  │                                 │  Matter Device  │
    │  (via Wi-Fi)    │                                 │  (via Thread)   │
    └─────────────────┘                                 └─────────────────┘


**Network Interfaces:**

* **Wi-Fi**: High-bandwidth connection for devices that require constant power
* **Thread**: Low-power mesh networking for battery-operated devices
* **Ethernet**: Wired connection for stationary devices

Matter Library Structure
------------------------

**The library is split into three main components:**

* ``Matter``: The main class that manages the Matter network and device commissioning
* ``MatterEndPoint``: The base class for all Matter endpoints, which provides common functionality for all endpoint types
* ``Specific endpoint classes``: The classes for all Matter endpoints, which provides the specific functionality for each endpoint type

Matter
******

The ``Matter`` class is the main entry point for all Matter operations. It serves as the central manager that handles:

* **Device Commissioning**: Managing the commissioning process via QR code or manual pairing code
* **Network Connectivity**: Checking and managing Wi-Fi and Thread connections
* **Event Handling**: Monitoring Matter events and device state changes
* **Device Management**: Decommissioning and factory reset functionality

The ``Matter`` class is implemented as a singleton, meaning there's only one instance available globally. You access it directly as ``Matter`` without creating an instance.

The ``Matter`` class provides the following key methods:

* ``begin()``: Initializes the Matter stack. On Wi-Fi station builds, starts the Wi-Fi driver first with reduced RX/TX buffers (4 static RX, 8 dynamic RX, 8 dynamic TX, AMPDU RX BA window 6) so CHIP inherits those counts, unless Thread or Ethernet was selected. Skipped if the sketch already called ``WiFi.begin()`` / ``WiFi.mode()``.
* ``isDeviceCommissioned()``: Checks if the device is commissioned (a fabric exists)
* ``isWiFiConnected()``: Checks Wi-Fi connection status
* ``isThreadConnected()``: Checks Thread connection status
* ``isDeviceConnected()``: Checks overall device connectivity (Wi-Fi, Thread, or Ethernet IPv6)
* ``isEthernetEnabled()``: ``CONFIG_ETH_ENABLED`` — ETH library builds; not "cable present"
* ``isNetworkSupported()``: Same compile-time check as the matching ``is*Enabled()``. ``NONE`` is false
* ``selectNetwork()``: Records intent before any accessory ``begin()``. Does not start Wi-Fi, Thread, or Ethernet, and does not apply a Thread dataset. One-argument form: Ethernet disables CHIPoBLE; Wi-Fi and Thread leave it on. ``selectNetwork(net, true)`` turns CHIPoBLE off; ``false`` does not turn it back on. ``NONE`` clears intent and does not change BLE
* ``getSelectedNetwork()``: Last successful ``selectNetwork()``, or ``NONE``
* ``getActiveNetwork()``: First netif with IPv6 (prefers the selection). Not ``isWiFiConnected()`` / ``isThreadConnected()``
* ``getNetworkEndPointId()``: Expected Network Commissioning endpoint (0 Wi-Fi, 0 Thread when Thread is selected, ``0xFFFF`` if none). Valid before the endpoint is created
* ``waitForNetwork()``: Blocks until that IPv6 is present. ``NONE`` waits for any interface. Does not start hardware. ``timeoutMs`` 0 is a single check
* ``isOnline()``: Checks if a controller has an active CASE session with this node. Stays true until CHIP idle-evicts that session, not until the user closes a controller app.
* ``isWiFiStationEnabled()``: Checks if Wi-Fi Station mode is supported and enabled
* ``isWiFiAccessPointEnabled()``: Checks if Wi-Fi AP mode is supported and enabled
* ``isThreadEnabled()``: ``CONFIG_ENABLE_MATTER_OVER_THREAD`` (C6/H2 prebuild). Not "OpenThread is compiled in" (C5)
* ``isBLECommissioningEnabled()``: Checks if BLE commissioning is compiled in **and** still enabled (see ``setBLECommissioningEnabled()``)
* ``setBLECommissioningEnabled()``: Enables or disables CHIPoBLE. Call before ``Matter.begin()``. Commissioning sketches use ``selectNetwork(net, true)`` instead of this setter. ``false`` is only when you keep the default network and just want BLE off (connect Wi-Fi or Ethernet first); it releases BLE RAM
* ``setBLEMemoryReleaseEnabled()``: After CHIPoBLE commissioning, release BLE RAM (default ``true``). Call before ``Matter.begin()``. Only takes effect when ``CONFIG_ENABLE_CHIPOBLE`` is set and CHIPoBLE commissioning is enabled. No effect when CHIPoBLE is compiled out. Arduino-as-IDF-component builds that keep BLE must also set ``CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=n``
* ``isBLEMemoryReleaseEnabled()``: ``true`` when CHIPoBLE is on and BLE RAM will be released after commissioning
* ``decommission()``: Factory resets the device
* ``getManualPairingCode()``: Gets the manual pairing code for commissioning (generated after ``begin()``; empty and a warning before ``begin()``)
* ``getOnboardingQRCodeUrl()``: Gets the QR code URL for commissioning (generated after ``begin()``; empty and a warning before ``begin()``)
* ``onEvent()``: Sets a callback for Matter events. The ``ChipDeviceEvent`` pointer is only valid during the callback; do not store it
* ``onBLEMemoryReleased()``: Called when CHIPoBLE BLE RAM has been returned to the heap (same moment as ``MATTER_BLE_DEINITIALIZED``). Register before ``Matter.begin()``. Runs on the CHIP task: do not block; allocate large buffers from ``loop()``. May never run if CHIPoBLE is off or ``setBLEMemoryReleaseEnabled(false)``

.. warning::

   A Matter sketch owns the CHIPoBLE host (NimBLE when that stack is selected). Do **not** use the Arduino ``BLE`` library (``BLE.h`` / ``BLEDevice``) in the same sketch. After Matter starts CHIPoBLE — or after it disables CHIPoBLE and releases BLE RAM, or after CHIPoBLE commissioning with ``setBLEMemoryReleaseEnabled(true)`` (the default) — ``BLEDevice::init()`` will fail or crash. ``setBLECommissioningEnabled(false)`` does not free the radio for Arduino BLE.

CHIPoBLE and NimBLE
^^^^^^^^^^^^^^^^^^^

Matter BLE commissioning is **CHIPoBLE**. The Arduino Matter APIs follow ``CONFIG_ENABLE_CHIPOBLE`` at build time, not a hardcoded SoC list.

* **CHIPoBLE** (``CONFIG_ENABLE_CHIPOBLE``): CHIP compiles the BLE commissioning layer and ``BLEMgr()``. Arduino ``isBLECommissioningEnabled()`` can be true only in this build.
* **NimBLE** (``CONFIG_BT_NIMBLE_ENABLED``): ESP-Matter selects the NimBLE ``BLEManagerImpl`` (host + controller). This is the stack used for CHIPoBLE RAM reclaim after ``BLEMgr().Shutdown()``.
* **Bluetooth** (``CONFIG_BT_ENABLED``): If Bluetooth is off in sdkconfig, CHIP disables CHIPoBLE. The Arduino setters remain in the API and return ``false`` / no-op as documented.

**Arduino IDE (precompiled Matter libraries)** follow the sdkconfig of each Matter static library. Columns below are those Kconfig flags, not silicon capability.

+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| SoC        | Default network               | Matter-over-Thread                | Ethernet  | CHIPoBLE (NimBLE)         | Notes            |
+============+===============================+===================================+===========+===========================+==================+
| ESP32      | Wi-Fi                         | No                                | Yes       | **No** (Bluedroid)        | EMAC or SPI PHY  |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| ESP32-S2   | Wi-Fi                         | No                                | SPI PHY   | **No** (no Bluetooth)     | On-network Wi-Fi |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| ESP32-S3   | Wi-Fi                         | No                                | SPI PHY   | Yes                       |                  |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| ESP32-C3   | Wi-Fi                         | No                                | SPI PHY   | Yes                       |                  |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| ESP32-C5   | Wi-Fi                         | No (radio exists; not in prebuild)| SPI PHY   | Yes                       |                  |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| ESP32-C6   | Wi-Fi until ``selectNetwork`` | Yes (dual-stack)                  | SPI PHY   | Yes                       |                  |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+
| ESP32-H2   | Thread                        | Yes                               | SPI PHY   | Yes                       | No Wi-Fi         |
+------------+-------------------------------+-----------------------------------+-----------+---------------------------+------------------+

``CONFIG_ENABLE_CHIPOBLE`` is off on original ESP32 and ESP32-S2. Those targets **must not** use CHIPoBLE sketches; ``setBLECommissioningEnabled(true)`` fails. Use `MatterOnNetworkWiFi <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkWiFi>`_ (or Ethernet). Example sketches that ``#if !CONFIG_ENABLE_CHIPOBLE`` call ``WiFi.begin()`` match this. Ethernet commissioning is always on-network: ``selectNetwork(ETHERNET)`` turns CHIPoBLE off. Wi-Fi and Thread leave CHIPoBLE on when it is compiled in; ``selectNetwork(net, true)`` turns it off.

Ethernet is available on **every Arduino Matter SoC** when you attach a PHY. SPI modules (W5500, DM9051, KSZ8851SNL) work on all of them — tested with original ESP32 + W5500. Internal RMII EMAC is original ESP32 only. There is no Ethernet Network Commissioning cluster; the sketch must ``ETH.begin()``, ``enableIPv6()``, and ``waitForNetwork()`` before ``Matter.begin()``. See `MatterOnNetworkEthernet <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkEthernet>`_.

**Arduino as an ESP-IDF component:** You choose the stacks in sdkconfig. Original ESP32 can run CHIPoBLE if you set ``CONFIG_BT_ENABLED=y``, ``CONFIG_BT_NIMBLE_ENABLED=y``, and ``CONFIG_ENABLE_CHIPOBLE=y``. The same sketch then uses BLE commissioning (no hardcoded Wi-Fi) because it keys off ``CONFIG_ENABLE_CHIPOBLE``. To keep NimBLE after commissioning, also set ``CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=n`` and call ``Matter.setBLEMemoryReleaseEnabled(false)`` before ``Matter.begin()``.

Waiting for BLE RAM
^^^^^^^^^^^^^^^^^^^

``BLEMgr().Shutdown()`` is asynchronous. Heap at ``Matter.begin()`` or at ``MATTER_COMMISSIONING_COMPLETE`` is not the post-reclaim size. ``Shutdown()`` only deinitializes the CHIPoBLE host; it does not return the RAM. CHIP reclaims it in ``BLEManagerImpl::ClaimBLEMemory()``, which it compiles in only when ``CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING`` is set — off in the precompiled Arduino libraries — and which has no release branch for every SoC even then. The Arduino Matter library therefore performs the reclaim itself: it waits for the NimBLE host task to exit, releases the BLE regions to the heap, then posts ``kBLEDeinitialized``. Both ``onBLEMemoryReleased()`` and ``MATTER_BLE_DEINITIALIZED`` fire from that event, exactly once, whichever path did the work.

Reclaim is not instant. Releasing the BLE regions while the NimBLE host task still runs would corrupt the heap, so the library polls for that task to exit every 2 seconds for up to 30 seconds. If the host never exits it logs an error and the callback does not run.

Register ``Matter.onBLEMemoryReleased()`` **before** ``Matter.begin()``. Use it to set a flag; allocate a large buffer from ``loop()``:

.. code-block:: arduino

    volatile bool bleRamFree = false;

    void setup() {
      Matter.onBLEMemoryReleased([]() { bleRamFree = true; });
      Light.begin();
      Matter.begin();
    }

    void loop() {
      static uint8_t *buf = nullptr;
      if (bleRamFree && buf == nullptr) {
        bleRamFree = false;
        buf = (uint8_t *)malloc(16 * 1024);
      }
    }

The callback may never run (no CHIPoBLE, ``setBLEMemoryReleaseEnabled(false)``, or a reclaim that timed out). Do not wait forever. ``onEvent()`` still delivers ``MATTER_BLE_DEINITIALIZED`` if you need other Matter events in the same sketch. See `MatterCHIPoBLERelease <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCHIPoBLERelease>`_.

Device identity
^^^^^^^^^^^^^^^

Call these setters **before** ``Matter.begin()``. After ``begin()`` they log a warning and have no effect. String setters copy into internal storage; the argument does not need to remain valid. ``setDeviceName()`` writes Basic Information **NodeLabel**. Do not change Vendor ID / Product ID from a sketch unless the DAC matches. SoftwareVersion is compile-time CHIP configuration.

.. code-block:: arduino

    Matter.setVendorName("Espressif");           // max 32
    Matter.setProductName("KitchenLight");       // max 32
    Matter.setDeviceName("KitchenHub");          // NodeLabel, max 32
    Matter.setSerialNumber("KH-000123");         // max 32
    Matter.setHardwareVersion(7);
    Matter.setHardwareVersionString("RevA");     // max 64
    Matter.setSetupDiscriminator(0xF01);         // 0–0xFFF; Arduino test default 0xF00
    Matter.setSetupPasscode(20202024);           // valid PIN; test default 20202021
    // Prefer selectNetwork(WIFI|THREAD, true) to pick a transport and turn CHIPoBLE off.
    // setBLECommissioningEnabled(false) only if you keep the default network and just want BLE off.
    Matter.setBLECommissioningEnabled(false);
    // Matter.setBLEMemoryReleaseEnabled(false); // only if CHIPoBLE is left on: keep NimBLE after commission
    Light.begin();
    Matter.begin();

On a single-endpoint node, controllers often use DeviceName as the accessory title. On a composed node it is the parent/node name; child lights are not renamed. Use ``MatterEndPoint::setTagList()`` for switch-style Descriptor tags, not as a light title.

``getManualPairingCode()`` and ``getOnboardingQRCodeUrl()`` are generated from the live discriminator and PIN after ``Matter.begin()``. Before ``begin()`` they log a warning and return an empty string. The 11-digit short manual code uses only the top 4 bits of the discriminator, so ``0xF00`` and ``0xF01`` collide if the PIN is unchanged. Changing the PIN requires a matching SPAKE2+ verifier; the library regenerates it. Test while uncommissioned and erase flash after changing codes. Production belongs in factory NVS with a unique PIN per unit.

Identity and commissioning APIs (all setters must run before ``Matter.begin()``):

* ``setVendorName()``
* ``setProductName()``
* ``setDeviceName()``
* ``setSerialNumber()``
* ``setHardwareVersion()``
* ``setHardwareVersionString()``
* ``setSetupDiscriminator()``
* ``setSetupPasscode()``
* ``setBLECommissioningEnabled()``
* ``setBLEMemoryReleaseEnabled()``
* ``selectNetwork()``
* ``waitForNetwork()``

Runtime status
^^^^^^^^^^^^^^

+-----------------------------------+--------------------------------------------------------------+
| API                               | Meaning                                                      |
+===================================+==============================================================+
| ``isDeviceCommissioned()``        | A Matter fabric exists                                       |
+-----------------------------------+--------------------------------------------------------------+
| ``isDeviceConnected()``           | Wi-Fi, Thread, or Ethernet IPv6 is up                        |
+-----------------------------------+--------------------------------------------------------------+
| ``isOnline()``                    | A controller has an active CASE session (until idle-evicted) |
+-----------------------------------+--------------------------------------------------------------+
| ``isBLECommissioningEnabled()``   | CHIPoBLE is compiled in and still enabled                    |
+-----------------------------------+--------------------------------------------------------------+
| ``isBLEMemoryReleaseEnabled()``   | CHIPoBLE is on and BLE RAM will be released after commission |
+-----------------------------------+--------------------------------------------------------------+
| ``isNetworkSupported()``          | Wi-Fi / Thread / Ethernet compiled in for this target        |
+-----------------------------------+--------------------------------------------------------------+
| ``getSelectedNetwork()``          | Last ``selectNetwork()`` value, or ``NONE``                  |
+-----------------------------------+--------------------------------------------------------------+
| ``getActiveNetwork()``            | Interface that currently has a usable IPv6 address           |
+-----------------------------------+--------------------------------------------------------------+

Do not gate physical outputs (LEDs) on ``isOnline()``. Restore last local state after commissioned. Use ``isOnline()`` for hub-discovery logs. A CASE session can remain active after the user leaves the app, until the hub or CHIP tears it down. See the Matter Status example.

Wi-Fi and Thread each have two commissioning paths. Do not mix them: CHIPoBLE plus a sketch SSID/dataset fights the hub; CHIPoBLE off plus no credentials is a dead end. Ethernet is on-network only (no commissioning cluster).

+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+
| Example                      | Transport | CHIPoBLE          | Credentials in the sketch                  | When to use                            |
+==============================+===========+===================+============================================+========================================+
| ``MatterCHIPoBLEWiFi``       | Wi-Fi     | On                | No. Hub sends SSID/password                | Factory-fresh Wi-Fi node               |
+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+
| ``MatterOnNetworkWiFi``      | Wi-Fi     | Off               | Yes. ``selectNetwork`` + ``WiFi.begin()``  | Already on Wi-Fi, or no BLE            |
+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+
| ``MatterCHIPoBLEThread``     | Thread    | On                | No. Hub sends the dataset                  | Factory-fresh Thread node (C6/H2)      |
+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+
| ``MatterOnNetworkThread``    | Thread    | Off               | Yes. Network key after ``Matter.begin()``  | Already on the mesh                    |
+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+
| ``MatterOnNetworkEthernet``  | Ethernet  | Off               | EMAC or SPI ``ETH.begin()`` + IPv6 first   | Wired only                             |
+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+
| ``MatterCHIPoBLERelease``    | Default   | On, then reclaimed| No                                         | BLE reclaim demo (Wi-Fi; Thread on H2) |
+------------------------------+-----------+-------------------+--------------------------------------------+----------------------------------------+

``MatterOnOffLight`` is the generic accessory demo (CHIPoBLE when compiled in, otherwise sketch Wi-Fi). Use the table when you care about the commissioning path.

Network selection
^^^^^^^^^^^^^^^^^

Call ``Matter.selectNetwork()`` **before any accessory** ``begin()``. With no call the selection is ``MATTER_NETWORK_NONE`` and existing sketches are unchanged.

+------------+----------------------------------------------+------------------+------------------------------------------------------+
| Network    | Supported when                               | CHIPoBLE default | Endpoint                                             |
+============+==============================================+==================+======================================================+
| Wi-Fi      | ``CONFIG_ENABLE_WIFI_STATION`` (not H2)      | Enabled          | 0 (primary)                                          |
+------------+----------------------------------------------+------------------+------------------------------------------------------+
| Thread     | ``CONFIG_ENABLE_MATTER_OVER_THREAD`` (C6/H2) | Enabled          | 0 when Thread is selected (C6 replaces root Wi-Fi)   |
+------------+----------------------------------------------+------------------+------------------------------------------------------+
| Ethernet   | ``CONFIG_ETH_ENABLED`` (wire hardware)       | Disabled         | None (``0xFFFF``). No commissioning cluster          |
+------------+----------------------------------------------+------------------+------------------------------------------------------+

* The library does **not** call ``ETH.begin()``. PHY macros are sketch-local. Internal EMAC (original ESP32, variant defines ``ETH_PHY_MDC`` / ``ETH_PHY_MDIO``): ``ETH.begin()``. SPI PHY (default W5500): ``SPI.begin()`` then ``ETH.begin(..., SPI)``. Then ``enableIPv6()`` and ``waitForNetwork()`` before ``Matter.begin()``.
* Do **not** start Arduino ``ESPmDNS``. CHIP owns the responder. ``MDNS.begin()`` overwrites the hostname; ``MDNS.end()`` destroys CHIP's services.
* ``selectNetwork(network, disableBLECommissioning)`` overrides the CHIPoBLE default. ``true`` turns CHIPoBLE off; do not also call ``setBLECommissioningEnabled()``. A selected-but-down interface plus no BLE leaves no commissioning path.
* Ethernet and Thread skip CHIP's ``InitWiFiStack()`` via a linker ``--wrap``.
* After ``Matter.begin()``, ``OThread.begin()`` attaches to CHIP's stack (``isAttachedToExternalStack()``). ``OThread.end()`` must not tear that stack down.
* C5: ``isThreadEnabled()`` is false until Matter-over-Thread is enabled in that prebuild.

See `MatterOnNetworkEthernet <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkEthernet>`_, `MatterCHIPoBLEWiFi <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCHIPoBLEWiFi>`_ / `MatterOnNetworkWiFi <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkWiFi>`_, and `MatterCHIPoBLEThread <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCHIPoBLEThread>`_ / `MatterOnNetworkThread <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkThread>`_.

MatterEndPoint
**************

The ``MatterEndPoint`` class is the base class for all Matter endpoints. It provides common functionality for all endpoint types.

* **Endpoint Management**: Each endpoint has a unique endpoint ID for identification
* **Attribute Access**: Methods to get and set attribute values from Matter clusters
* **Identify Cluster**: Support for device identification (visual feedback)
* **Secondary Network Interfaces**: Support for multiple network interfaces (Wi-Fi, Thread, Ethernet)
* **Attribute Change Callbacks**: Base framework for handling attribute changes from Matter controllers

.. toctree::
    :maxdepth: 2

    matter_ep

Specific endpoint classes
*************************

The library provides specialized endpoint classes for different device types. Each endpoint type includes specific clusters and functionality relevant to that device category.

**Lighting Endpoints:**

* ``MatterOnOffLight``: Simple on/off light control
* ``MatterDimmableLight``: Light with brightness control
* ``MatterColorTemperatureLight``: Light with brightness and color temperature (no RGB wheel)
* ``MatterColorLight``: RGB color light (HSV/XY, no color temperature slider in Matter APPs)
* ``MatterEnhancedColorLight``: Extended color light with RGB, brightness, and color temperature

**Sensor Endpoints:**

* ``MatterTemperatureSensor``: Temperature sensor (read-only)
* ``MatterHumiditySensor``: Humidity sensor (read-only)
* ``MatterPressureSensor``: Pressure sensor (read-only)
* ``MatterContactSensor``: Contact sensor (open/closed state)
* ``MatterWaterLeakDetector``: Water leak detector (detected/not detected state)
* ``MatterWaterFreezeDetector``: Water freeze detector (detected/not detected state)
* ``MatterRainSensor``: Rain sensor (detected/not detected state)
* ``MatterOccupancySensor``: Occupancy sensor (occupied/unoccupied state)
* ``MatterLightSensor``: Light sensor (read-only)

**Control Endpoints:**

* ``MatterTemperatureControlledCabinet``: Temperature controlled cabinet (setpoint control with min/max limits)

* ``MatterFan``: Fan with speed and mode control
* ``MatterThermostat``: Thermostat with temperature control and setpoints
* ``MatterOnOffPlugin``: On/off plugin unit (power outlet/relay)
* ``MatterDimmablePlugin``: Dimmable plugin unit (power outlet/relay with brightness control)
* ``MatterGenericSwitch``: Generic switch endpoint (smart button with optional long-press and multi-press)
* ``MatterWindowCovering``: Window covering with lift and tilt control (blinds, shades)

.. toctree::
    :maxdepth: 1
    :glob:

    ep_*

Matter Examples
---------------

The Matter library includes a comprehensive set of examples demonstrating various device types and use cases. All examples are available in the `ESP Arduino GitHub repository <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples>`_.

**Getting Started:**

* **Matter Minimum** - The smallest code required to create a Matter-compatible device. Ideal starting point for understanding Matter basics. `View Matter Minimum code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/GettingStarted/MatterMinimum>`_
* **Matter Status** - Demonstrates how to check enabled Matter features and connectivity status, including ``isDeviceCommissioned()``, ``isDeviceConnected()``, and ``isOnline()``. Implements a basic on/off light and periodically reports capability and connection status. `View Matter Status code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/GettingStarted/MatterStatus>`_
* **Matter Device Identity** - Sets VendorName, ProductName, DeviceName (NodeLabel), SerialNumber, hardware version, and custom commissioning codes on ``Matter`` before ``begin()``. `View Matter Device Identity code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/GettingStarted/MatterDeviceIdentity>`_
* **Matter Events** - Shows how to monitor and handle Matter events. Provides a comprehensive view of all Matter events during device operation. `View Matter Events code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/GettingStarted/MatterEvents>`_

**Commissioning:**

* **Matter CHIPoBLE Wi-Fi** - CHIPoBLE on, no SSID in the sketch. The hub sends Wi-Fi credentials. `View Matter CHIPoBLE Wi-Fi code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCHIPoBLEWiFi>`_
* **Matter On-Network Wi-Fi** - CHIPoBLE off, ``selectNetwork(WIFI, true)`` then ``WiFi.begin(ssid, password)``. `View Matter On-Network Wi-Fi code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkWiFi>`_
* **Matter CHIPoBLE Thread** - CHIPoBLE on, no dataset. The hub sends the Thread dataset (C6/H2). `View Matter CHIPoBLE Thread code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCHIPoBLEThread>`_
* **Matter On-Network Thread** - CHIPoBLE off, sketch network key after ``Matter.begin()``. `View Matter On-Network Thread code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkThread>`_
* **Matter On-Network Ethernet** - CHIPoBLE off, EMAC or SPI ``ETH.begin()`` and IPv6 before ``Matter.begin()``. `View Matter On-Network Ethernet code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterOnNetworkEthernet>`_
* **Matter CHIPoBLE Release** - CHIPoBLE commissioning, then BLE RAM reclaim. Uses ``onBLEMemoryReleased()`` and allocates a demo buffer from ``loop()``. `View Matter CHIPoBLE Release code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCHIPoBLERelease>`_
* **Matter Commission Test** - Tests Matter commissioning functionality with automatic decommissioning after a 30-second delay for continuous testing cycles. `View Matter Commission Test code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Commissioning/MatterCommissionTest>`_

**Lighting Examples:**

* **Matter On/Off Light** - Creates a Matter-compatible on/off light device with commissioning, device control via smart home ecosystems, and manual control using a physical button with state persistence. `View Matter On/Off Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterOnOffLight>`_
* **Matter Dimmable Light** - Creates a Matter-compatible dimmable light device with brightness control. `View Matter Dimmable Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterDimmableLight>`_
* **Matter Color Temperature Light** - Creates a Matter-compatible color temperature light device with adjustable color temperature control. `View Matter Color Temperature Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterTemperatureLight>`_
* **Matter Color Light** - Creates a Matter-compatible RGB color light (HSV/XY, no color temperature). `View Matter Color Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterColorLight>`_
* **Matter Enhanced Color Light** - Creates a Matter-compatible extended color light with RGB, brightness, and color temperature. `View Matter Enhanced Color Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterEnhancedColorLight>`_
* **Matter Composed Lights** - Creates a Matter node with multiple light endpoints (On/Off Light, Dimmable Light, and Color Light) in a single node. `View Matter Composed Lights code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterComposedLights>`_
* **Matter On Identify** - Implements the Matter Identify cluster callback for an on/off light device, making the LED blink when the device is identified from a Matter app. `View Matter On Identify code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Lighting/MatterOnIdentify>`_

**Sensor Examples:**

* **Matter Temperature Sensor** - Creates a Matter-compatible temperature sensor device with sensor data reporting to smart home ecosystems. `View Matter Temperature Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterTemperatureSensor>`_
* **Matter Humidity Sensor** - Creates a Matter-compatible humidity sensor device with sensor data reporting. `View Matter Humidity Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterHumiditySensor>`_
* **Matter Pressure Sensor** - Creates a Matter-compatible pressure sensor device with automatic simulation of pressure readings. `View Matter Pressure Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterPressureSensor>`_
* **Matter Contact Sensor** - Creates a Matter-compatible contact sensor device (open/closed state). `View Matter Contact Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterContactSensor>`_
* **Matter Occupancy Sensor** - Creates a Matter-compatible occupancy sensor device with automatic simulation of occupancy state changes. `View Matter Occupancy Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterOccupancySensor>`_
* **Matter Occupancy Sensor with HoldTime** - Creates a Matter-compatible occupancy sensor device with HoldTime functionality, automatic simulation of occupancy state changes, HoldTime configuration with persistence across reboots, and HoldTime change callback for real-time updates from Matter controllers. `View Matter Occupancy Sensor with HoldTime code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterOccupancyWithHoldTime>`_
* **Matter Water Leak Detector** - Creates a Matter-compatible water leak detector device with automatic simulation of water leak detection state changes. `View Matter Water Leak Detector code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterWaterLeakDetector>`_
* **Matter Water Freeze Detector** - Creates a Matter-compatible water freeze detector device with automatic simulation of water freeze detection state changes. `View Matter Water Freeze Detector code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterWaterFreezeDetector>`_
* **Matter Rain Sensor** - Creates a Matter-compatible rain sensor device with automatic simulation of rain detection state changes. `View Matter Rain Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterRainSensor>`_
* **Matter Light Sensor** - Creates a Matter-compatible light (illuminance) sensor device with automatic simulation of illuminance readings. `View Matter Light Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Sensors/MatterLightSensor>`_

**Control Examples:**

* **Matter Fan** - Creates a Matter-compatible fan device with speed and mode control. `View Matter Fan code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterFan>`_
* **Matter Thermostat** - Creates a Matter-compatible thermostat device with temperature setpoint management and simulated heating/cooling systems with automatic temperature regulation. `View Matter Thermostat code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterThermostat>`_
* **Matter Temperature Controlled Cabinet** - Creates a Matter-compatible temperature controlled cabinet device with precise temperature setpoint control with min/max limits (temperature_number mode). `View Matter Temperature Controlled Cabinet code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterTemperatureControlledCabinet>`_
* **Matter Temperature Controlled Cabinet Levels** - Creates a Matter-compatible temperature controlled cabinet device using predefined temperature levels (temperature_level mode). `View Matter Temperature Controlled Cabinet Levels code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterTemperatureControlledCabinetLevels>`_
* **Matter On/Off Plugin** - Creates a Matter-compatible on/off plugin unit (power relay) device with state persistence for power control applications. `View Matter On/Off Plugin code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterOnOffPlugin>`_
* **Matter Dimmable Plugin** - Creates a Matter-compatible dimmable plugin unit (power outlet with level control) device with state persistence for dimmable power control applications. `View Matter Dimmable Plugin code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterDimmablePlugin>`_
* **Matter Smart Button** - One Generic Switch. Short click only (``InitialPress`` + ``ShortRelease``). `View Matter Smart Button code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterSmartButton>`_
* **Matter Enhanced Smart Button** - Still one physical button (BOOT). Enables Switch ``FEATURE_ALL`` and sends ``LongPress`` / ``LongRelease`` and ``MultiPressOngoing`` / ``MultiPressComplete``. Not extra tagged buttons. `View Matter Enhanced Smart Button code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterEnhancedSmartButton>`_
* **Matter Smart Buttons TagList** - Three short-click Generic Switches (On, Off, Scene) told apart with Descriptor ``TagList``. No long-press or multi-press. `View Matter Smart Buttons TagList code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterSmartButtonsTagList>`_
* **Matter Window Covering** - Creates a Matter-compatible window covering device with lift and tilt control (blinds, shades) with manual control using a physical button. `View Matter Window Covering code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterWindowCovering>`_
* **Matter Simple Blinds** - A minimal example that only controls lift percentage using a single onGoToLiftPercentage() callback. `View Matter Simple Blinds code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Control/MatterSimpleBlinds>`_

**Advanced Examples:**

* **Matter Lambda Single Callback Many Endpoints** - Demonstrates how to create multiple Matter endpoints in a single node using a shared lambda function callback with capture for efficient callback handling. `View Matter Lambda Single Callback Many Endpoints code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/Advanced/MatterLambdaSingleCallbackManyEPs>`_

Common Problems and Issues
--------------------------

Troubleshooting
---------------

Common Issues
*************

**Device won't commission**
  * Ensure the Matter hub is in pairing mode
  * Check that Wi-Fi or Thread connectivity is properly configured
  * Verify the QR code or pairing code is correct
  * For builds without CHIPoBLE (``CONFIG_ENABLE_CHIPOBLE=n``: Arduino IDE on original ESP32 or ESP32-S2, or Bluetooth off), set Wi-Fi credentials in the sketch. Original ESP32 can use CHIPoBLE when built as an IDF component with NimBLE and ``CONFIG_ENABLE_CHIPOBLE``.

**Commissioning fails**
  * Try factory resetting the device by calling ``Matter.decommission()``
  * Erase flash memory: ``Arduino IDE Menu`` -> ``Tools`` -> ``Erase All Flash Before Sketch Upload: "Enabled"``
  * Or use esptool: ``esptool.py --port <PORT> erase_flash``

**Wi-Fi connection issues**
  * Verify Wi-Fi credentials (SSID and password) are correct
  * Check that the device is within range of the Wi-Fi router
  * Ensure the Wi-Fi network is 2.4 GHz (Matter requires 2.4 GHz Wi-Fi)

**Thread connection issues**
  * Verify Thread border router is properly configured
  * Check that Thread network credentials are correct
  * Ensure device supports Thread (ESP32-H2, ESP32-C6 with Thread enabled)

**Device not responding**
  * Check Serial Monitor for error messages (115200 baud)
  * Verify endpoint initialization with ``begin()`` method
  * Ensure ``Matter.begin()`` is called after all endpoints are initialized
  * For Boolean State sensors (contact, leak, freeze, rain), call the setter after ``Matter.begin()``. ``begin()`` does not take an initial state.

**Callbacks not firing**
  * Verify callback functions are registered before commissioning
  * Check that callback functions are properly defined
  * Ensure endpoint is properly initialized with ``begin()``

Factory Reset
*************

If you have problems with commissioning or device connectivity, you can try to factory reset the device. This will erase all the Matter network settings and act as a brand new device.

.. code-block:: arduino

    Matter.decommission();

This will reset the device and it will need to be commissioned again.

Event Monitoring
****************

For debugging and monitoring Matter events, you can set up an event callback:

.. code-block:: arduino

    Matter.onEvent([](matterEvent_t event, const chip::DeviceLayer::ChipDeviceEvent *eventData) {
        Serial.printf("Matter Event: 0x%04X\r\n", event);
        // Handle specific events
        switch(event) {
            case MATTER_COMMISSIONING_COMPLETE:
                Serial.println("Device commissioned!");
                break;
            case MATTER_WIFI_CONNECTIVITY_CHANGE:
                Serial.println("Wi-Fi connectivity changed");
                break;
            // ... handle other events
        }
    });

This allows you to monitor commissioning progress, connectivity changes, and other Matter events in real-time.

To run code only after BLE RAM is back on the heap, use ``Matter.onBLEMemoryReleased()`` (see **Waiting for BLE RAM** above). ``MATTER_BLE_DEINITIALIZED`` is the matching ``onEvent()`` type.
