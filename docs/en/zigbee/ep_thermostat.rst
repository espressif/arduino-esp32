################
ZigbeeThermostat
################

About
-----

The ``ZigbeeThermostat`` class provides a thermostat endpoint for Zigbee networks.
This endpoint implements the Zigbee Home Automation (HA) standard for thermostats that can bind to temperature and humidity sensors and receive temperature and humidity readings.

**Features:**
* Automatic discovery and binding to temperature sensors
* Temperature and humidity data reception from bound sensors
* Configurable temperature and humidity reporting intervals
* Sensor settings retrieval (min/max/tolerance for temperature and humidity)
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

onTempConfigReceive
^^^^^^^^^^^^^^^^^^^

Sets a callback function for receiving sensor configuration data.

.. code-block:: arduino

    void onTempConfigReceive(void (*callback)(float min_temp, float max_temp, float tolerance));

* ``callback`` - Function to call when sensor configuration is received
* ``min_temp`` - Minimum temperature supported by the sensor
* ``max_temp`` - Maximum temperature supported by the sensor
* ``tolerance`` - Temperature tolerance of the sensor

onHumidityReceive
^^^^^^^^^^^^^^^^^

Sets a callback function for receiving humidity data.

.. code-block:: arduino

    void onHumidityReceive(void (*callback)(float humidity));

* ``callback`` - Function to call when humidity data is received
* ``humidity`` - Humidity value in percentage

onHumidityReceiveWithSource
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback function for receiving humidity data with source information.

.. code-block:: arduino

    void onHumidityReceiveWithSource(void (*callback)(float humidity, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address));

* ``callback`` - Function to call when humidity data is received
* ``humidity`` - Humidity value in percentage
* ``src_endpoint`` - Source endpoint that sent the humidity data
* ``src_address`` - Source address information

onHumidityConfigReceive
^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback function for receiving humidity sensor configuration data.

.. code-block:: arduino

    void onHumidityConfigReceive(void (*callback)(float min_humidity, float max_humidity, float tolerance));

* ``callback`` - Function to call when humidity sensor configuration is received
* ``min_humidity`` - Minimum humidity supported by the sensor
* ``max_humidity`` - Maximum humidity supported by the sensor
* ``tolerance`` - Humidity tolerance of the sensor

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature data from a specific endpoint using short address.

.. code-block:: arduino

    void getTemperature(uint8_t endpoint, uint16_t short_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device

getTemperature (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature data from a specific endpoint using IEEE address.

.. code-block:: arduino

    void getTemperature(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device

Humidity Data Retrieval
***********************

getHumidity
^^^^^^^^^^^

Requests humidity data from all bound sensors.

.. code-block:: arduino

    void getHumidity();

getHumidity (Group)
^^^^^^^^^^^^^^^^^^^

Requests humidity data from a specific group.

.. code-block:: arduino

    void getHumidity(uint16_t group_addr);

* ``group_addr`` - Group address to send the request to

getHumidity (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests humidity data from a specific endpoint using short address.

.. code-block:: arduino

    void getHumidity(uint8_t endpoint, uint16_t short_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device

getHumidity (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests humidity data from a specific endpoint using IEEE address.

.. code-block:: arduino

    void getHumidity(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device

Temperature Settings Retrieval
******************************

getTemperatureSettings
^^^^^^^^^^^^^^^^^^^^^^

Requests temperature sensor settings from all bound sensors.

.. code-block:: arduino

    void getTemperatureSettings();

getTemperatureSettings (Group)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature sensor settings from a specific group.

.. code-block:: arduino

    void getTemperatureSettings(uint16_t group_addr);

* ``group_addr`` - Group address to send the request to

getTemperatureSettings (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature sensor settings from a specific endpoint using short address.

.. code-block:: arduino

    void getTemperatureSettings(uint8_t endpoint, uint16_t short_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device

getTemperatureSettings (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests temperature sensor settings from a specific endpoint using IEEE address.

.. code-block:: arduino

    void getTemperatureSettings(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device

Humidity Settings Retrieval
***************************

getHumiditySettings
^^^^^^^^^^^^^^^^^^^

Requests humidity sensor settings from all bound sensors.

.. code-block:: arduino

    void getHumiditySettings();

getHumiditySettings (Group)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests humidity sensor settings from a specific group.

.. code-block:: arduino

    void getHumiditySettings(uint16_t group_addr);

* ``group_addr`` - Group address to send the request to

getHumiditySettings (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests humidity sensor settings from a specific endpoint using short address.

.. code-block:: arduino

    void getHumiditySettings(uint8_t endpoint, uint16_t short_addr);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device

getHumiditySettings (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Requests humidity sensor settings from a specific endpoint using IEEE address.

.. code-block:: arduino

    void getHumiditySettings(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

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

Humidity Reporting Configuration
********************************

setHumidityReporting
^^^^^^^^^^^^^^^^^^^^

Configures humidity reporting for all bound sensors.

.. code-block:: arduino

    void setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in humidity to trigger a report

setHumidityReporting (Group)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configures humidity reporting for a specific group.

.. code-block:: arduino

    void setHumidityReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);

* ``group_addr`` - Group address to configure
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in humidity to trigger a report

setHumidityReporting (Endpoint + Short Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configures humidity reporting for a specific endpoint using short address.

.. code-block:: arduino

    void setHumidityReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);

* ``endpoint`` - Target endpoint number
* ``short_addr`` - Short address of the target device
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in humidity to trigger a report

setHumidityReporting (Endpoint + IEEE Address)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Configures humidity reporting for a specific endpoint using IEEE address.

.. code-block:: arduino

    void setHumidityReporting(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta);

* ``endpoint`` - Target endpoint number
* ``ieee_addr`` - IEEE address of the target device
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in humidity to trigger a report

Example
-------

Thermostat Implementation
*************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Thermostat/Zigbee_Thermostat.ino
    :language: arduino
