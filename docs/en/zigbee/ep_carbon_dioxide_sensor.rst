#########################
ZigbeeCarbonDioxideSensor
#########################

About
-----

The ``ZigbeeCarbonDioxideSensor`` class provides a CO2 sensor endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for carbon dioxide measurement devices.

API Reference
-------------

Constructor
***********

ZigbeeCarbonDioxideSensor
^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee CO2 sensor endpoint.

.. code-block:: arduino

    ZigbeeCarbonDioxideSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setCarbonDioxide
^^^^^^^^^^^^^^^^

Sets the CO2 concentration measurement value.

.. code-block:: arduino

    bool setCarbonDioxide(float carbon_dioxide);

* ``carbon_dioxide`` - CO2 concentration value in ppm

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(float min, float max);

* ``min`` - Minimum CO2 concentration value in ppm
* ``max`` - Maximum CO2 concentration value in ppm

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(float tolerance);

* ``tolerance`` - Tolerance value in ppm

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for CO2 measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report in ppm

**Note:** Delta reporting is currently not supported by the carbon dioxide sensor.

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current CO2 concentration value.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

CO2 Sensor Implementation
*************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_CarbonDioxide_Sensor/Zigbee_CarbonDioxide_Sensor.ino
    :language: arduino
