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

Building and Flashing Matter Examples
--------------------------------------

Before uploading any Matter example sketch, it is necessary to configure the Arduino IDE with the following settings:

<!-- vale off -->
1. **Partition Scheme**: Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.

  .. figure:: ../../_static/matter_partition_scheme.png
      :align: center
      :alt: "Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)" Arduino IDE menu option
      :figclass: align-center
<!-- vale on -->

2. **Erase Flash**: Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.

  .. figure:: ../../_static/matter_erase_flash.png
      :align: center
      :alt: "Erase All Flash Before Sketch Upload: Enabled" Arduino IDE menu option
      :figclass: align-center

These settings are required for the following reasons:

* **Partition Scheme**: Matter firmware requires a large application partition (3MB) to accommodate the Matter stack and application code.
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

* ``begin()``: Initializes the Matter stack
* ``isDeviceCommissioned()``: Checks if the device is commissioned
* ``isWiFiConnected()``: Checks Wi-Fi connection status
* ``isThreadConnected()``: Checks Thread connection status
* ``isDeviceConnected()``: Checks overall device connectivity
* ``isWiFiStationEnabled()``: Checks if Wi-Fi Station mode is supported and enabled
* ``isWiFiAccessPointEnabled()``: Checks if Wi-Fi AP mode is supported and enabled
* ``isThreadEnabled()``: Checks if Thread network is supported and enabled
* ``isBLECommissioningEnabled()``: Checks if BLE commissioning is supported and enabled
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
* ``MatterWaterLeakDetector``: Water leak detector (detected/not detected state)
* ``MatterWaterFreezeDetector``: Water freeze detector (detected/not detected state)
* ``MatterRainSensor``: Rain sensor (detected/not detected state)
* ``MatterOccupancySensor``: Occupancy sensor (occupied/unoccupied state)

**Control Endpoints:**

* ``MatterTemperatureControlledCabinet``: Temperature controlled cabinet (setpoint control with min/max limits)

* ``MatterFan``: Fan with speed and mode control
* ``MatterThermostat``: Thermostat with temperature control and setpoints
* ``MatterOnOffPlugin``: On/off plugin unit (power outlet/relay)
* ``MatterDimmablePlugin``: Dimmable plugin unit (power outlet/relay with brightness control)
* ``MatterGenericSwitch``: Generic switch endpoint (smart button)
* ``MatterWindowCovering``: Window covering with lift and tilt control (blinds, shades)

.. toctree::
    :maxdepth: 1
    :glob:

    ep_*

Matter Examples
---------------

The Matter library includes a comprehensive set of examples demonstrating various device types and use cases. All examples are available in the `ESP Arduino GitHub repository <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples>`_.

**Basic Examples:**

* **Matter Minimum** - The smallest code required to create a Matter-compatible device. Ideal starting point for understanding Matter basics. `View Matter Minimum code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterMinimum>`_
* **Matter Status** - Demonstrates how to check enabled Matter features and connectivity status. Implements a basic on/off light and periodically reports capability and connection status. `View Matter Status code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterStatus>`_
* **Matter Events** - Shows how to monitor and handle Matter events. Provides a comprehensive view of all Matter events during device operation. `View Matter Events code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEvents>`_
* **Matter Commission Test** - Tests Matter commissioning functionality with automatic decommissioning after a 30-second delay for continuous testing cycles. `View Matter Commission Test code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterCommissionTest>`_

**Lighting Examples:**

* **Matter On/Off Light** - Creates a Matter-compatible on/off light device with commissioning, device control via smart home ecosystems, and manual control using a physical button with state persistence. `View Matter On/Off Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterOnOffLight>`_
* **Matter Dimmable Light** - Creates a Matter-compatible dimmable light device with brightness control. `View Matter Dimmable Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterDimmableLight>`_
* **Matter Color Temperature Light** - Creates a Matter-compatible color temperature light device with adjustable color temperature control. `View Matter Color Temperature Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterTemperatureLight>`_
* **Matter Color Light** - Creates a Matter-compatible color light device with RGB color control (HSV color model). `View Matter Color Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterColorLight>`_
* **Matter Enhanced Color Light** - Creates a Matter-compatible enhanced color light with color temperature and brightness control. `View Matter Enhanced Color Light code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterEnhancedColorLight>`_
* **Matter Composed Lights** - Creates a Matter node with multiple light endpoints (On/Off Light, Dimmable Light, and Color Light) in a single node. `View Matter Composed Lights code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterComposedLights>`_
* **Matter On Identify** - Implements the Matter Identify cluster callback for an on/off light device, making the LED blink when the device is identified from a Matter app. `View Matter On Identify code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterOnIdentify>`_

**Sensor Examples:**

* **Matter Temperature Sensor** - Creates a Matter-compatible temperature sensor device with sensor data reporting to smart home ecosystems. `View Matter Temperature Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterTemperatureSensor>`_
* **Matter Humidity Sensor** - Creates a Matter-compatible humidity sensor device with sensor data reporting. `View Matter Humidity Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterHumiditySensor>`_
* **Matter Pressure Sensor** - Creates a Matter-compatible pressure sensor device with automatic simulation of pressure readings. `View Matter Pressure Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterPressureSensor>`_
* **Matter Contact Sensor** - Creates a Matter-compatible contact sensor device (open/closed state). `View Matter Contact Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterContactSensor>`_
* **Matter Occupancy Sensor** - Creates a Matter-compatible occupancy sensor device with automatic simulation of occupancy state changes. `View Matter Occupancy Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterOccupancySensor>`_
* **Matter Water Leak Detector** - Creates a Matter-compatible water leak detector device with automatic simulation of water leak detection state changes. `View Matter Water Leak Detector code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterWaterLeakDetector>`_
* **Matter Water Freeze Detector** - Creates a Matter-compatible water freeze detector device with automatic simulation of water freeze detection state changes. `View Matter Water Freeze Detector code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterWaterFreezeDetector>`_
* **Matter Rain Sensor** - Creates a Matter-compatible rain sensor device with automatic simulation of rain detection state changes. `View Matter Rain Sensor code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterRainSensor>`_

**Control Examples:**

* **Matter Fan** - Creates a Matter-compatible fan device with speed and mode control. `View Matter Fan code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterFan>`_
* **Matter Thermostat** - Creates a Matter-compatible thermostat device with temperature setpoint management and simulated heating/cooling systems with automatic temperature regulation. `View Matter Thermostat code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterThermostat>`_
* **Matter Temperature Controlled Cabinet** - Creates a Matter-compatible temperature controlled cabinet device with precise temperature setpoint control with min/max limits (temperature_number mode). `View Matter Temperature Controlled Cabinet code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterTemperatureControlledCabinet>`_
* **Matter Temperature Controlled Cabinet Levels** - Creates a Matter-compatible temperature controlled cabinet device using predefined temperature levels (temperature_level mode). `View Matter Temperature Controlled Cabinet Levels code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterTemperatureControlledCabinetLevels>`_
* **Matter On/Off Plugin** - Creates a Matter-compatible on/off plugin unit (power relay) device with state persistence for power control applications. `View Matter On/Off Plugin code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterOnOffPlugin>`_
* **Matter Dimmable Plugin** - Creates a Matter-compatible dimmable plugin unit (power outlet with level control) device with state persistence for dimmable power control applications. `View Matter Dimmable Plugin code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterDimmablePlugin>`_
* **Matter Smart Button** - Creates a Matter-compatible smart button (generic switch) device that sends button click events to smart home ecosystems and triggers automations. `View Matter Smart Button code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSmartButton>`_
* **Matter Window Covering** - Creates a Matter-compatible window covering device with lift and tilt control (blinds, shades) with manual control using a physical button. `View Matter Window Covering code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterWindowCovering>`_
* **Matter Simple Blinds** - A minimal example that only controls lift percentage using a single onGoToLiftPercentage() callback. `View Matter Simple Blinds code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterSimpleBlinds>`_

**Advanced Examples:**

* **Matter Lambda Single Callback Many Endpoints** - Demonstrates how to create multiple Matter endpoints in a single node using a shared lambda function callback with capture for efficient callback handling. `View Matter Lambda Single Callback Many Endpoints code on GitHub <https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples/MatterLambdaSingleCallbackManyEPs>`_

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
