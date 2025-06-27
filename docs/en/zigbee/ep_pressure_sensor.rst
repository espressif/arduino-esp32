####################
ZigbeePressureSensor
####################

About
-----

The ``ZigbeePressureSensor`` class provides an endpoint for pressure sensors in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for pressure measurement devices, supporting atmospheric pressure, barometric pressure, and other pressure measurements.

**Features:**
* Pressure measurement in hPa (hectopascals)
* Configurable measurement range
* Tolerance and reporting configuration
* Automatic reporting capabilities

API Reference
-------------

Constructor
***********

ZigbeePressureSensor
^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee pressure sensor endpoint.

.. code-block:: arduino

    ZigbeePressureSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setPressure
^^^^^^^^^^^

Sets the pressure measurement value.

.. code-block:: arduino

    bool setPressure(int16_t value);

* ``value`` - Pressure value in hPa

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(int16_t min, int16_t max);

* ``min`` - Minimum pressure value in hPa
* ``max`` - Maximum pressure value in hPa

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(uint16_t tolerance);

* ``tolerance`` - Tolerance value in hPa

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for pressure measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report in hPa

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current pressure value.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Pressure + Flow Sensor Implementation
*************************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Pressure_Flow_Sensor/Zigbee_Pressure_Flow_Sensor.ino
    :language: arduino
