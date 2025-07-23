######
Zigbee
######

About
-----

The Zigbee library provides support for creating Zigbee 3.0 compatible devices including:

* Support for different Zigbee roles (Coordinator, Router, End Device)
* Network management (scanning, joining, commissioning)
* Multiple endpoint types for various device categories
* OTA (Over-The-Air) update support
* Power management for battery-powered devices
* Time synchronization
* Advanced binding and group management

The Zigbee library is built on top of `ESP-ZIGBEE-SDK <https://github.com/espressif/esp-zigbee-sdk>`_ and provides a high-level Arduino-style interface for creating Zigbee devices.

Zigbee Network Topology
***********************

.. code-block:: text

    ┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
    │   Coordinator   │◄─────►│     Router      │◄─────►│     Router      │
    │   (Gateway)     │       │   (Repeater)    │       │  (Thermostat)   │
    └─────────────────┘       └─────────────────┘       └─────────────────┘
            │                          │                         │
            │                          ▼                         │
            │                 ┌─────────────────┐                │
            │                 │   End Device    │                │
            │                 │    (Sensor)     │                │
            │                 └─────────────────┘                │
            │                                                    │
            ▼                                                    ▼
    ┌─────────────────┐                                 ┌─────────────────┐
    │   End Device    │                                 │   End Device    │
    │   (Sensor)      │                                 │    (Sensor)     │
    └─────────────────┘                                 └─────────────────┘


**Device Roles**

* **Coordinator**: Forms and manages the network, stores network information
* **Router**: Extends network range, routes messages, mains powered devices (typically lights, switches, etc.)
* **End Device**: Battery-powered devices that can sleep for extended periods (typically sensors)

Zigbee Library Structure
------------------------

**The library is split into three main components:**

* ``ZigbeeCore``: The main class that manages the Zigbee network
* ``ZigbeeEP``: The base class for all Zigbee endpoints, which provides common functionality for all endpoint types
* ``Specific endpoint classes``: The classes for all Zigbee endpoints, which provides the specific functionality for each endpoint type

ZigbeeCore
**********

The ``ZigbeeCore`` class is the main entry point for all Zigbee operations. It serves as the central coordinator that manages:

* **Network Operations**: Starting, stopping, and managing the Zigbee network
* **Device Role Management**: Configuring the device as Coordinator, Router, or End Device
* **Endpoint Management**: Adding and managing multiple device endpoints
* **Network Discovery**: Scanning for and joining existing networks
* **OTA Updates**: Managing over-the-air firmware updates
* **Power Management**: Configuring sleep modes for battery-powered devices

The ``ZigbeeCore`` class is implemented as a singleton, meaning there's only one instance available globally. You access it directly as ``Zigbee`` without creating an instance.

.. toctree::
    :maxdepth: 3

    zigbee_core

ZigbeeEP
********

The ``ZigbeeEP`` class is the base class for all Zigbee endpoints. It provides common functionality for all endpoint types.

* **Device Information**: Every endpoint can be configured with manufacturer and model information that helps identify the device on the network
* **Binding Management**: Binding allows endpoints to establish direct communication links with other devices. This enables automatic command transmission without requiring manual addressing
* **Power Management**: Endpoints can report their power source type and battery status, which helps the network optimize communication patterns
* **Time Synchronization**: Endpoints can synchronize with network time, enabling time-based operations and scheduling
* **OTA Support**: Endpoints can receive over-the-air firmware updates to add new features or fix bugs
* **Device Discovery**: Endpoints can read manufacturer and model information from other devices on the network

.. toctree::
    :maxdepth: 2

    zigbee_ep

Specific endpoint classes
*************************

Library provides the following endpoint classes from lights, switches, sensors, etc. Each endpoint class provides the specific functionality for each endpoint type and inherits from the ``ZigbeeEP`` class.

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

**Device won't join network**
  * Ensure the coordinator is in pairing mode
  * Check that the device is configured with the correct role
  * Verify the channel mask includes the coordinator's channel

**OTA updates fail**
  * Ensure the OTA server is properly configured
  * Check that the device has sufficient memory for the update
  * Verify network connectivity

**Battery devices not working**
  * Ensure proper power source configuration
  * Check sleep/wake timing settings
  * Verify parent device (router/coordinator) is always powered

**Binding issues**
  * Check that both devices support the required clusters
  * Verify that binding is enabled on both devices
  * Ensure devices are on the same network

**Network connectivity problems**
  * Check that devices are within range
  * Verify that routers are properly configured
  * Check for interference from other 2.4 GHz devices

Factory Reset
*************

If you have problems with connecting to the network, you can try to factory reset the device. This will erase all the network settings and act as brand new device.

.. code-block:: arduino

    Zigbee.factoryReset(true); // true = restart after reset

Debug Mode
**********

For better debugging, you can enable debug mode to get detailed information about network operations. Call debug mode before starting Zigbee.
Also selecting zigbee mode with *debug* suffix is recommended.

.. code-block:: arduino

    Zigbee.setDebugMode(true);
    // Start Zigbee with debug output
    Zigbee.begin();
