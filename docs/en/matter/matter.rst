######
Matter
######

About
-----

The Matter library provides support for creating Matter-compatible devices including:

* Support for Wi-Fi and Thread connectivity
* Matter commissioning via QR code or manual pairing code
* Multiple endpoint types for various device categories
* Event monitoring and callback support
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Smart home ecosystem compatibility

The Matter library is built on top of `ESP Matter SDK <https://github.com/espressif/esp-matter>`_ and provides a high-level Arduino-style interface for creating Matter devices.

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

* ``begin()``: Initializes the Matter stack
* ``isDeviceCommissioned()``: Checks if the device is commissioned
* ``isWi-FiConnected()``: Checks Wi-Fi connection status (if Wi-Fi is enabled)
* ``isThreadConnected()``: Checks Thread connection status (if Thread is enabled)
* ``isDeviceConnected()``: Checks overall device connectivity
* ``decommission()``: Factory resets the device
* ``getManualPairingCode()``: Gets the manual pairing code for commissioning
* ``getOnboardingQRCodeUrl()``: Gets the QR code URL for commissioning
* ``onEvent()``: Sets a callback for Matter events

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
* ``MatterColorTemperatureLight``: Light with color temperature control
* ``MatterColorLight``: Light with RGB color control (HSV color model)
* ``MatterEnhancedColorLight``: Enhanced color light with color temperature and brightness control

**Sensor Endpoints:**

* ``MatterTemperatureSensor``: Temperature sensor (read-only)
* ``MatterHumiditySensor``: Humidity sensor (read-only)
* ``MatterPressureSensor``: Pressure sensor (read-only)
* ``MatterContactSensor``: Contact sensor (open/closed state)
<<<<<<< Updated upstream
=======
* ``MatterRainSensor``: Rain sensor (detected/not detected state)
* ``MatterWaterFreezeDetector``: Water freeze detector (detected/not detected state)
>>>>>>> Stashed changes
* ``MatterOccupancySensor``: Occupancy sensor (occupied/unoccupied state)

**Control Endpoints:**

* ``MatterFan``: Fan with speed and mode control
* ``MatterThermostat``: Thermostat with temperature control and setpoints
* ``MatterOnOffPlugin``: On/off plugin unit (power outlet/relay)
* ``MatterGenericSwitch``: Generic switch endpoint (smart button)

.. toctree::
    :maxdepth: 1
    :glob:

    ep_*

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
  * For ESP32/ESP32-S2, ensure Wi-Fi credentials are set in the code

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
