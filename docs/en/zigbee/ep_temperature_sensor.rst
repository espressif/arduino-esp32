################
ZigbeeTempSensor
################

About
-----

The ``ZigbeeTempSensor`` class provides a temperature and humidity sensor endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for environmental monitoring with configurable reporting intervals and thresholds.

**Features:**
* Temperature measurement and reporting
* Optional humidity measurement
* Configurable reporting intervals
* Min/max value and tolerance settings

API Reference
-------------

Constructor
***********

ZigbeeTempSensor
^^^^^^^^^^^^^^^^

Creates a new Zigbee temperature sensor endpoint.

.. code-block:: arduino

    ZigbeeTempSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Temperature Control
*******************

setTemperature
^^^^^^^^^^^^^^

Sets the temperature value in 0.01°C resolution.

.. code-block:: arduino

    bool setTemperature(float value);

* ``value`` - Temperature value in degrees Celsius

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum temperature values for the sensor.

.. code-block:: arduino

    bool setMinMaxValue(float min, float max);

* ``min`` - Minimum temperature value in degrees Celsius
* ``max`` - Maximum temperature value in degrees Celsius

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for temperature reporting.

.. code-block:: arduino

    bool setTolerance(float tolerance);

* ``tolerance`` - Tolerance value in degrees Celsius

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting interval for temperature measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in temperature to trigger report (in 0.01°C)

This function will return ``true`` if successful, ``false`` otherwise.

reportTemperature
^^^^^^^^^^^^^^^^^

Manually reports the current temperature value.

.. code-block:: arduino

    bool reportTemperature();

This function will return ``true`` if successful, ``false`` otherwise.

Humidity Control (Optional)
***************************

addHumiditySensor
^^^^^^^^^^^^^^^^^

Adds humidity measurement capability to the temperature sensor.

.. code-block:: arduino

    void addHumiditySensor(float min, float max, float tolerance);

* ``min`` - Minimum humidity value in percentage
* ``max`` - Maximum humidity value in percentage
* ``tolerance`` - Tolerance value in percentage

setHumidity
^^^^^^^^^^^

Sets the humidity value in 0.01% resolution.

.. code-block:: arduino

    bool setHumidity(float value);

* ``value`` - Humidity value in percentage (0-100)

This function will return ``true`` if successful, ``false`` otherwise.

setHumidityReporting
^^^^^^^^^^^^^^^^^^^^

Sets the reporting interval for humidity measurements.

.. code-block:: arduino

    bool setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in humidity to trigger report (in 0.01%)

This function will return ``true`` if successful, ``false`` otherwise.

reportHumidity
^^^^^^^^^^^^^^

Manually reports the current humidity value.

.. code-block:: arduino

    bool reportHumidity();

This function will return ``true`` if successful, ``false`` otherwise.

Combined Reporting
******************

report
^^^^^^

Reports both temperature and humidity values if humidity sensor is enabled.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Temperature Sensor Implementation
*********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Temperature_Sensor/Zigbee_Temperature_Sensor.ino
    :language: arduino

Temperature + Humidity Sleepy Sensor Implementation
***************************************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Temp_Hum_Sensor_Sleepy/Zigbee_Temp_Hum_Sensor_Sleepy.ino
    :language: arduino
