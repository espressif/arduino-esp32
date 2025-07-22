#####################
ZigbeeWindSpeedSensor
#####################

About
-----

The ``ZigbeeWindSpeedSensor`` class provides a wind speed sensor endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for wind speed measurement devices.

**Features:**
* Wind speed measurement in m/s
* Configurable measurement range
* Tolerance and reporting configuration
* Automatic reporting capabilities
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Weather stations
* Wind turbine monitoring
* Agricultural weather monitoring
* Marine applications
* Smart home weather systems
* Industrial wind monitoring

API Reference
-------------

Constructor
***********

ZigbeeWindSpeedSensor
^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee wind speed sensor endpoint.

.. code-block:: arduino

    ZigbeeWindSpeedSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setWindSpeed
^^^^^^^^^^^^

Sets the wind speed measurement value.

.. code-block:: arduino

    bool setWindSpeed(float value);

* ``value`` - Wind speed value in 0.01 m/s

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(float min, float max);

* ``min`` - Minimum wind speed value in 0.01 m/s
* ``max`` - Maximum wind speed value in 0.01 m/s

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(float tolerance);

* ``tolerance`` - Tolerance value in 0.01 m/s

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for wind speed measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report in 0.01 m/s

This function will return ``true`` if successful, ``false`` otherwise.

reportWindSpeed
^^^^^^^^^^^^^^^

Manually reports the current wind speed value.

.. code-block:: arduino

    bool reportWindSpeed();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Wind Speed Sensor Implementation
********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Wind_Speed_Sensor/Zigbee_Wind_Speed_Sensor.ino
    :language: arduino
