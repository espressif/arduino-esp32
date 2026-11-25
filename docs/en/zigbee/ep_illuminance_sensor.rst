#######################
ZigbeeIlluminanceSensor
#######################

About
-----

The ``ZigbeeIlluminanceSensor`` class provides an endpoint for illuminance sensors in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for light level measurement devices, supporting ambient light monitoring.

**Features:**
* Raw ZCL Illuminance Measurement value, not lux. Approximate lux is ``10^((value - 1) / 10000)``
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

Sets the raw ZCL illuminance measurement value. The value is stored as given and is not converted from lux.

.. code-block:: arduino

    bool setIlluminance(uint16_t value);

* ``value`` - Raw ZCL illuminance value, not lux

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(uint16_t min, uint16_t max);

* ``min`` - Minimum raw ZCL illuminance value, not lux
* ``max`` - Maximum raw ZCL illuminance value, not lux

This function will return ``true`` if successful, ``false`` otherwise.

setDefaultValue
^^^^^^^^^^^^^^^

Sets the default (initial) value for the illuminance sensor. This value will be used as the initial measured value when the device is in factory reset mode and before the sensor provides actual readings.

.. code-block:: arduino

    bool setDefaultValue(uint16_t defaultValue);

* ``defaultValue`` - Raw ZCL illuminance value, not lux

**Important:** Must be called before adding the EP to Zigbee class. Only effective when the device is in factory reset mode (before commissioning/joining a network).

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(uint16_t tolerance);

* ``tolerance`` - Raw ZCL illuminance tolerance, not lux

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for illuminance measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in the raw ZCL illuminance value required to report

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
