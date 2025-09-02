########
ZigbeeEP
########

About
-----

The ``ZigbeeEP`` class is the base class for all Zigbee endpoints. It provides common functionality for all endpoint types.

* **Device Information**: Every endpoint can be configured with manufacturer and model information that helps identify the device on the network
* **Binding Management**: Binding allows endpoints to establish direct communication links with other devices. This enables automatic command transmission without requiring manual addressing
* **Power Management**: Endpoints can report their power source type and battery status, which helps the network optimize communication patterns
* **Time Synchronization**: Endpoints can synchronize with network time, enabling time-based operations and scheduling
* **OTA Support**: Endpoints can receive over-the-air firmware updates to add new features or fix bugs
* **Device Discovery**: Endpoints can read manufacturer and model information from other devices on the network


ZigbeeEP APIs
-------------

Device Information
******************

setManufacturerAndModel
^^^^^^^^^^^^^^^^^^^^^^^

Sets the manufacturer name and model identifier for the device.

.. code-block:: arduino

    bool setManufacturerAndModel(const char *name, const char *model);

* ``name`` - Manufacturer name (max 32 characters)
* ``model`` - Model identifier (max 32 characters)

This function will return ``true`` if set successfully, ``false`` otherwise.

getEndpoint
^^^^^^^^^^^

Gets the endpoint number assigned to this device.

.. code-block:: arduino

    uint8_t getEndpoint();

This function will return the endpoint number (1-254).

Binding Management
******************

bound
^^^^^

Checks if the endpoint has any bound devices.

.. code-block:: arduino

    bool bound();

This function will return ``true`` if the endpoint has bound devices, ``false`` otherwise.

getBoundDevices
^^^^^^^^^^^^^^^

Gets the list of devices bound to this endpoint.

.. code-block:: arduino

    std::vector<esp_zb_binding_info_t> getBoundDevices();

This function will return list of bound device parameters.

printBoundDevices
^^^^^^^^^^^^^^^^^

Prints information about bound devices to Serial or a custom Print object.

.. code-block:: arduino

    void printBoundDevices(Print &print = Serial);

* ``print`` - Custom Print object (optional, defaults to Serial)

allowMultipleBinding
^^^^^^^^^^^^^^^^^^^^

Enables or disables multiple device binding for this endpoint.

.. code-block:: arduino

    void allowMultipleBinding(bool bind);

* ``bind`` - ``true`` to allow multiple bindings, ``false`` for single binding only

setManualBinding
^^^^^^^^^^^^^^^^

Enables or disables manual binding mode. Manual binding mode is supposed to be used when using ZHA or Z2M where you bind devices manually.

.. code-block:: arduino

    void setManualBinding(bool bind);

* ``bind`` - ``true`` for manual binding, ``false`` for automatic binding (default)

clearBoundDevices
^^^^^^^^^^^^^^^^^

Removes all bound devices from this endpoint.

.. code-block:: arduino

    void clearBoundDevices();

Binding Status
**************

epAllowMultipleBinding
^^^^^^^^^^^^^^^^^^^^^^

Gets whether multiple device binding is allowed for this endpoint.

.. code-block:: arduino

    bool epAllowMultipleBinding();

This function will return ``true`` if multiple bindings are allowed, ``false`` otherwise.

epUseManualBinding
^^^^^^^^^^^^^^^^^^

Gets whether manual binding mode is enabled for this endpoint.

.. code-block:: arduino

    bool epUseManualBinding();

This function will return ``true`` if manual binding is enabled, ``false`` otherwise.

Power Management
****************

setPowerSource
^^^^^^^^^^^^^^

Sets the power source type for the endpoint.

.. code-block:: arduino

    bool setPowerSource(uint8_t source, uint8_t percentage = 0xff, uint8_t voltage = 0xff);

* ``source`` - Power source type (``ZB_POWER_SOURCE_MAINS``, ``ZB_POWER_SOURCE_BATTERY``, etc.)
* ``percentage`` - Battery percentage (0-100, default: 0xff)
* ``voltage`` - Battery voltage in 100 mV units (default: 0xff)

This function will return ``true`` if set successfully, ``false`` otherwise.

setBatteryPercentage
^^^^^^^^^^^^^^^^^^^^

Sets the current battery percentage.

.. code-block:: arduino

    bool setBatteryPercentage(uint8_t percentage);

* ``percentage`` - Battery percentage (0-100)

This function will return ``true`` if set successfully, ``false`` otherwise.

setBatteryVoltage
^^^^^^^^^^^^^^^^^

Sets the battery voltage.

.. code-block:: arduino

    bool setBatteryVoltage(uint8_t voltage);

* ``voltage`` - Battery voltage in 100 mV units (e.g., 35 for 3.5 V)

This function will return ``true`` if set successfully, ``false`` otherwise.

reportBatteryPercentage
^^^^^^^^^^^^^^^^^^^^^^^

Reports the current battery percentage to the network.

.. code-block:: arduino

    bool reportBatteryPercentage();

This function will return ``true`` if reported successfully, ``false`` otherwise.

Time Synchronization
********************

addTimeCluster
^^^^^^^^^^^^^^

Adds time synchronization cluster to the endpoint. When you want to add a server cluster (have the time and GMT offset) fill the time structure with the current time and GMT offset.
For client cluster (get the time and GMT offset) keep the default parameters.

.. code-block:: arduino

    bool addTimeCluster(tm time = {}, int32_t gmt_offset = 0);

* ``time`` - Current time structure (default: empty)
* ``gmt_offset`` - GMT offset in seconds (default: 0)

This function will return ``true`` if added successfully, ``false`` otherwise.

setTime
^^^^^^^

Sets the current time for the endpoint.

.. code-block:: arduino

    bool setTime(tm time);

* ``time`` - Time structure to set

This function will return ``true`` if set successfully, ``false`` otherwise.

setTimezone
^^^^^^^^^^^

Sets the timezone offset for the endpoint.

.. code-block:: arduino

    bool setTimezone(int32_t gmt_offset);

* ``gmt_offset`` - GMT offset in seconds

This function will return ``true`` if set successfully, ``false`` otherwise.

getTime
^^^^^^^

Gets the current network time.

.. code-block:: arduino

    struct tm getTime(uint8_t endpoint = 1, int32_t short_addr = 0x0000, esp_zb_ieee_addr_t ieee_addr = {});

* ``endpoint`` - Target endpoint (default: 1)
* ``short_addr`` - Target device short address (default: 0x0000)
* ``ieee_addr`` - Target device IEEE address (default: empty)

This function will return network time structure.

getTimezone
^^^^^^^^^^^

Gets the timezone offset.

.. code-block:: arduino

    int32_t getTimezone(uint8_t endpoint = 1, int32_t short_addr = 0x0000, esp_zb_ieee_addr_t ieee_addr = {});

* ``endpoint`` - Target endpoint (default: 1)
* ``short_addr`` - Target device short address (default: 0x0000)
* ``ieee_addr`` - Target device IEEE address (default: empty)

This function will return GMT offset in seconds.

OTA Support
***********

addOTAClient
^^^^^^^^^^^^

Adds OTA client to the endpoint for firmware updates.

.. code-block:: arduino

    bool addOTAClient(uint32_t file_version, uint32_t downloaded_file_ver, uint16_t hw_version, uint16_t manufacturer = 0x1001, uint16_t image_type = 0x1011, uint8_t max_data_size = 223);

* ``file_version`` - Current firmware version
* ``downloaded_file_ver`` - Downloaded file version
* ``hw_version`` - Hardware version
* ``manufacturer`` - Manufacturer code (default: 0x1001)
* ``image_type`` - Image type code (default: 0x1011)
* ``max_data_size`` - Maximum data size for OTA transfer (default: 223)

This function will return ``true`` if added successfully, ``false`` otherwise.

requestOTAUpdate
^^^^^^^^^^^^^^^^

Requests OTA update from the server.

.. code-block:: arduino

    void requestOTAUpdate();

Device Discovery
****************

readManufacturer
^^^^^^^^^^^^^^^^

Reads the manufacturer name from a remote device.

.. code-block:: arduino

    char *readManufacturer(uint8_t endpoint, uint16_t short_addr, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Target device short address
* ``ieee_addr`` - Target device IEEE address

This function will return pointer to manufacturer string, or ``NULL`` if read failed.

readModel
^^^^^^^^^

Reads the model identifier from a remote device.

.. code-block:: arduino

    char *readModel(uint8_t endpoint, uint16_t short_addr, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Target device short address
* ``ieee_addr`` - Target device IEEE address

This function will return pointer to model string, or ``NULL`` if read failed.

Event Handling
**************

onIdentify
^^^^^^^^^^

Sets a callback function for identify events.

.. code-block:: arduino

    void onIdentify(void (*callback)(uint16_t));

* ``callback`` - Function to call when identify event occurs
* ``time`` - Identify time in seconds

onDefaultResponse
^^^^^^^^^^^^^^^^^

Sets a callback for default response messages for this endpoint.

.. code-block:: arduino

    void onDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, esp_zb_zcl_status_t status));

* ``callback`` - Function pointer to the callback function

This callback will be called when a default response is received for this specific endpoint.

Supported Endpoints
-------------------

The Zigbee library provides specialized endpoint classes for different device types. Each endpoint type includes specific clusters and functionality relevant to that device category.

.. toctree::
    :maxdepth: 1
    :glob:

    ep_*
