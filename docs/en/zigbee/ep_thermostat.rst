################
ZigbeeThermostat
################

About
-----

The ``ZigbeeThermostat`` class provides a thermostat endpoint for Zigbee networks that receives temperature data from temperature sensors. This endpoint implements the Zigbee Home Automation (HA) standard for thermostats that can bind to temperature sensors and receive temperature readings.

**Features:**
* Automatic discovery and binding to temperature sensors
* Temperature data reception from bound sensors
* Configurable temperature reporting intervals
* Sensor settings retrieval (min/max temperature, tolerance)
* Multiple addressing modes (group, specific endpoint, IEEE address)

API Reference
-------------

Constructor
***********

ZigbeeThermostat
^^^^^^^^^^^^^^^^

Creates a new Zigbee thermostat endpoint.

.. code-block:: arduino

    ZigbeeThermostat(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Event Handling
**************

onTempReceive
^^^^^^^^^^^^^

Sets a callback function for receiving temperature data.

.. code-block:: arduino

    void onTempReceive(void (*callback)(float temperature));

* ``callback`` - Function to call when temperature data is received
* ``temperature`` - Temperature value in degrees Celsius

onTempReceiveWithSource
^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback function for receiving temperature data with source information.

.. code-block:: arduino

    void onTempReceiveWithSource(void (*callback)(float temperature, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address));

* ``callback`` - Function to call when temperature data is received
* ``temperature`` - Temperature value in degrees Celsius
* ``src_endpoint`` - Source endpoint that sent the temperature data
* ``src_address`` - Source address information

onConfigReceive
^^^^^^^^^^^^^^^

Sets a callback function for receiving sensor configuration data.

.. code-block:: arduino

    void onConfigReceive(void (*callback)(float min_temp, float max_temp, float tolerance));

* ``callback`` - Function to call when sensor configuration is received
* ``min_temp`` - Minimum temperature supported by the sensor
* ``max_temp`` - Maximum temperature supported by the sensor
* ``tolerance`` - Temperature tolerance of the sensor

Temperature Data Retrieval
**************************

getTemperature
^^^^^^^^^^^^^^

Requests temperature data from all bound sensors.

.. code-block:: arduino

    void getTemperature();

getTemperature (Group)
^^^^^^^^^^^^^^^^^^^^^^

Requests temperature data from a specific group.

.. code-block:: arduino

    void getTemperature(uint16_t group_addr);

* ``group_addr`` - Group address to send the request to

getTemperature (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature data from a specific endpoint using short address.

.. code-block:: arduino

    void getTemperature(uint8_t endpoint, uint16_t short_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device

getTemperature (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature data from a specific endpoint using IEEE address.

.. code-block:: arduino

    void getTemperature(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device

Sensor Settings Retrieval
*************************

getSensorSettings
^^^^^^^^^^^^^^^^^

Requests sensor settings from all bound sensors.

.. code-block:: arduino

    void getSensorSettings();

getSensorSettings (Group)
^^^^^^^^^^^^^^^^^^^^^^^^^

Requests sensor settings from a specific group.

.. code-block:: arduino

    void getSensorSettings(uint16_t group_addr);

* ``group_addr`` - Group address to send the request to

getSensorSettings (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests sensor settings from a specific endpoint using short address.

.. code-block:: arduino

    void getSensorSettings(uint8_t endpoint, uint16_t short_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device

getSensorSettings (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests sensor settings from a specific endpoint using IEEE address.

.. code-block:: arduino

    void getSensorSettings(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device

Temperature Reporting Configuration
***********************************

setTemperatureReporting
^^^^^^^^^^^^^^^^^^^^^^^

Configures temperature reporting for all bound sensors.

.. code-block:: arduino

    void setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in temperature to trigger a report

setTemperatureReporting (Group)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configures temperature reporting for a specific group.

.. code-block:: arduino

    void setTemperatureReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);

* ``group_addr`` - Group address to configure
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in temperature to trigger a report

setTemperatureReporting (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configures temperature reporting for a specific endpoint using short address.

.. code-block:: arduino

    void setTemperatureReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in temperature to trigger a report

setTemperatureReporting (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configures temperature reporting for a specific endpoint using IEEE address.

.. code-block:: arduino

    void setTemperatureReporting(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in temperature to trigger a report

Example
-------

Thermostat Implementation
*************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Thermostat/Zigbee_Thermostat.ino
    :language: arduino
