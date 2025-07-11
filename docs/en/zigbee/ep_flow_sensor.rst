################
ZigbeeFlowSensor
################

About
-----

The ``ZigbeeFlowSensor`` class provides a flow sensor endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for liquid and gas flow measurement devices.

**Features:**
* Flow rate measurement in m³/h
* Configurable measurement range
* Tolerance and reporting configuration
* Automatic reporting capabilities
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Water flow monitoring
* Gas flow measurement
* Industrial process monitoring
* Smart home water management
* HVAC system flow monitoring
* Agricultural irrigation systems

API Reference
-------------

Constructor
***********

ZigbeeFlowSensor
^^^^^^^^^^^^^^^^

Creates a new Zigbee flow sensor endpoint.

.. code-block:: arduino

    ZigbeeFlowSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setFlow
^^^^^^^

Sets the flow rate measurement value.

.. code-block:: arduino

    bool setFlow(float value);

* ``value`` - Flow rate value in 0.1 m³/h

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(float min, float max);

* ``min`` - Minimum flow rate value in 0.1 m³/h
* ``max`` - Maximum flow rate value in 0.1 m³/h

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(float tolerance);

* ``tolerance`` - Tolerance value in 0.01 m³/h

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for flow rate measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report in 0.1 m³/h

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current flow rate value.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Flow + PressureSensor Implementation
************************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Pressure_Flow_Sensor/Zigbee_Pressure_Flow_Sensor.ino
    :language: arduino
