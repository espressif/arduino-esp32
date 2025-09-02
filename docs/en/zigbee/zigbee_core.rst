##########
ZigbeeCore
##########

About
-----

The ``ZigbeeCore`` class is the main entry point for all Zigbee operations. It serves as the central class that manages:

* **Network Operations**: Starting, stopping, and managing the Zigbee network
* **Device Role Management**: Configuring the device as Coordinator, Router, or End Device
* **Endpoint Management**: Adding and managing multiple device endpoints
* **Network Discovery**: Scanning for and joining existing networks

ZigbeeCore APIs
---------------

Network Initialization
**********************

begin
^^^^^

Initializes the Zigbee stack and starts the network.

.. code-block:: arduino

    bool begin(zigbee_role_t role = ZIGBEE_END_DEVICE, bool erase_nvs = false);
    bool begin(esp_zb_cfg_t *role_cfg, bool erase_nvs = false);

* ``role`` - Device role (default: ``ZIGBEE_END_DEVICE``)
* ``role_cfg`` - Custom role configuration structure
* ``erase_nvs`` - Whether to erase NVS storage (default: ``false``)

This function will return ``true`` if initialization successful, ``false`` otherwise.

**Available Roles:**

* **ZIGBEE_COORDINATOR**: Network coordinator, forms and manages the network
* **ZIGBEE_ROUTER**: Network router, connects to existing network and extends network range and routes messages (if device is mains powered, always use this role)
* **ZIGBEE_END_DEVICE**: End device, connects to existing network (typically battery-powered which can sleep)

.. note::

    Depending on the Zigbee role, proper Zigbee mode and partition scheme must be set in the Arduino IDE.

    * **ZIGBEE_COORDINATOR** and **ZIGBEE_ROUTER**:
        * Zigbee mode to ``Zigbee ZCZR (coordinator/router)``.
        * Partition scheme to ``Zigbee ZCZR xMB with spiffs`` (where ``x`` is the number of MB of selected flash size).
    * **ZIGBEE_END_DEVICE**:
        * Zigbee mode to ``Zigbee ED (end device)``.
        * Partition scheme to ``Zigbee xMB with spiffs`` (where ``x`` is the number of MB of selected flash size).

Network Status
**************

started
^^^^^^^

Checks if the Zigbee stack has been started.

.. code-block:: arduino

    bool started();

This function will return ``true`` if Zigbee stack is running, ``false`` otherwise.

connected
^^^^^^^^^

Checks if the device is connected to a Zigbee network.

.. code-block:: arduino

    bool connected();

This function will return ``true`` if connected to network, ``false`` otherwise.

getRole
^^^^^^^

Gets the current Zigbee device role.

.. code-block:: arduino

    zigbee_role_t getRole();

This function will return current device role (``ZIGBEE_COORDINATOR``, ``ZIGBEE_ROUTER``, ``ZIGBEE_END_DEVICE``).

Endpoint Management
*******************

addEndpoint
^^^^^^^^^^^

Adds an endpoint to the Zigbee network.

.. code-block:: arduino

    bool addEndpoint(ZigbeeEP *ep);

* ``ep`` - Pointer to the endpoint object to add

This function will return ``true`` if endpoint added successfully, ``false`` otherwise.

Network Configuration
*********************

setPrimaryChannelMask
^^^^^^^^^^^^^^^^^^^^^

Sets the primary channel mask for network scanning and joining.

.. code-block:: arduino

    void setPrimaryChannelMask(uint32_t mask);

* ``mask`` - Channel mask (default: all channels 11-26, mask 0x07FFF800)

setScanDuration
^^^^^^^^^^^^^^^

Sets the scan duration for network discovery.

.. code-block:: arduino

    void setScanDuration(uint8_t duration);

* ``duration`` - Scan duration (1-4, where 1 is fastest, 4 is slowest)

getScanDuration
^^^^^^^^^^^^^^^

Gets the current scan duration setting.

.. code-block:: arduino

    uint8_t getScanDuration();

This function will return current scan duration (1-4).

Power Management
****************

setRxOnWhenIdle
^^^^^^^^^^^^^^^

Sets whether the device keeps its receiver on when idle.

.. code-block:: arduino

    void setRxOnWhenIdle(bool rx_on_when_idle);

* ``rx_on_when_idle`` - ``true`` to keep receiver on, ``false`` to allow sleep

getRxOnWhenIdle
^^^^^^^^^^^^^^^

Gets the current receiver idle setting.

.. code-block:: arduino

    bool getRxOnWhenIdle();

This function will return current receiver idle setting.

setTimeout
^^^^^^^^^^

Sets the timeout for network operations.

.. code-block:: arduino

    void setTimeout(uint32_t timeout);

* ``timeout`` - Timeout in milliseconds (default: 30000 ms)

Network Discovery
*****************

scanNetworks
^^^^^^^^^^^^

Scans for available Zigbee networks.

.. code-block:: arduino

    void scanNetworks(uint32_t channel_mask = ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK, uint8_t scan_duration = 5);

* ``channel_mask`` - Channels to scan (default: all channels)
* ``scan_duration`` - Scan duration (default: 5)

scanComplete
^^^^^^^^^^^^

Checks if network scanning is complete.

.. code-block:: arduino

    int16_t scanComplete();

This function will return:
* ``-2``: Scan failed or not started
* ``-1``: Scan running
* ``0``: No networks found
* ``>0``: Number of networks found

getScanResult
^^^^^^^^^^^^^

Gets the scan results.

.. code-block:: arduino

    zigbee_scan_result_t *getScanResult();

This function will return pointer to scan results, or ``NULL`` if no results.

scanDelete
^^^^^^^^^^

Deletes the scan results from memory.

.. code-block:: arduino

    void scanDelete();

Network Management (Coordinator only)
*************************************

setRebootOpenNetwork
^^^^^^^^^^^^^^^^^^^^

Opens the network for joining after reboot for a specified time.

.. code-block:: arduino

    void setRebootOpenNetwork(uint8_t time);

* ``time`` - Time in seconds to keep network open after reboot

openNetwork
^^^^^^^^^^^

Opens the network for device joining for a specified time.

.. code-block:: arduino

    void openNetwork(uint8_t time);

* ``time`` - Time in seconds to keep network open for device joining

closeNetwork
^^^^^^^^^^^^

Closes the network to prevent new devices from joining.

.. code-block:: arduino

    void closeNetwork();

Radio Configuration
*******************

setRadioConfig
^^^^^^^^^^^^^^

Sets the radio configuration.

.. code-block:: arduino

    void setRadioConfig(esp_zb_radio_config_t config);

* ``config`` - Radio configuration structure

getRadioConfig
^^^^^^^^^^^^^^

Gets the current radio configuration.

.. code-block:: arduino

    esp_zb_radio_config_t getRadioConfig();

This function will return current radio configuration.

Host Configuration
******************

setHostConfig
^^^^^^^^^^^^^

Sets the host configuration.

.. code-block:: arduino

    void setHostConfig(esp_zb_host_config_t config);

* ``config`` - Host configuration structure

getHostConfig
^^^^^^^^^^^^^

Gets the current host configuration.

.. code-block:: arduino

    esp_zb_host_config_t getHostConfig();

This function will return current host configuration.

Debug and Utilities
*******************

setDebugMode
^^^^^^^^^^^^

Enables or disables debug mode.

.. code-block:: arduino

    void setDebugMode(bool debug);

* ``debug`` - ``true`` to enable debug output, ``false`` to disable

getDebugMode
^^^^^^^^^^^^

Gets the current debug mode setting.

.. code-block:: arduino

    bool getDebugMode();

This function will return current debug mode setting.

factoryReset
^^^^^^^^^^^^

Performs a factory reset, clearing all network settings.

.. code-block:: arduino

    void factoryReset(bool restart = true);

* ``restart`` - ``true`` to restart after reset (default: ``true``)

onGlobalDefaultResponse
^^^^^^^^^^^^^^^^^^^^^^^

Sets a global callback for default response messages.

.. code-block:: arduino

    void onGlobalDefaultResponse(void (*callback)(zb_cmd_type_t resp_to_cmd, esp_zb_zcl_status_t status, uint8_t endpoint, uint16_t cluster));

* ``callback`` - Function pointer to the callback function

This callback will be called for all endpoints when a default response is received.

Utility Functions
*****************

formatIEEEAddress
^^^^^^^^^^^^^^^^^

Formats an IEEE address for display.

.. code-block:: arduino

    static const char *formatIEEEAddress(const esp_zb_ieee_addr_t addr);

* ``addr`` - IEEE address to format

This function will return formatted address string.

formatShortAddress
^^^^^^^^^^^^^^^^^^

Formats a short address for display.

.. code-block:: arduino

    static const char *formatShortAddress(uint16_t addr);

* ``addr`` - Short address to format

This function will return formatted address string.
