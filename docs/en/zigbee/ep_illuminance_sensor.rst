#######################
ZigbeeIlluminanceSensor
#######################

About
-----

The ``ZigbeeIlluminanceSensor`` class provides an endpoint for illuminance sensors in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for light level measurement devices, supporting ambient light monitoring.

**Features:**
* Illuminance measurement in lux
* Configurable measurement range
* Tolerance and reporting configuration
* Automatic reporting capabilities

API Reference
-------------

Constructor
***********

ZigbeeIlluminanceSensor
^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee illuminance sensor endpoint.

.. code-block:: arduino

    ZigbeeIlluminanceSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setIlluminance
^^^^^^^^^^^^^^

Sets the illuminance measurement value.

.. code-block:: arduino

    bool setIlluminance(uint16_t value);

* ``value`` - Illuminance value in lux

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(uint16_t min, uint16_t max);

* ``min`` - Minimum illuminance value in lux
* ``max`` - Maximum illuminance value in lux

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(uint16_t tolerance);

* ``tolerance`` - Tolerance value in lux

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for illuminance measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report in lux

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current illuminance value.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Illuminance Sensor Implementation
*********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Illuminance_Sensor/Zigbee_Illuminance_Sensor.ino
    :language: arduino
