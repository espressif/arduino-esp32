#####################
ZigbeeOccupancySensor
#####################

About
-----

The ``ZigbeeOccupancySensor`` class provides an endpoint for occupancy sensors in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for occupancy detection devices, supporting various sensor types for detecting presence.

**Features:**
* Occupancy detection (occupied/unoccupied)
* Multiple sensor type support (PIR, ultrasonic, etc.)
* Automatic reporting capabilities
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

API Reference
-------------

Constructor
***********

ZigbeeOccupancySensor
^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee occupancy sensor endpoint.

.. code-block:: arduino

    ZigbeeOccupancySensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setOccupancy
^^^^^^^^^^^^

Sets the occupancy state.

.. code-block:: arduino

    bool setOccupancy(bool occupied);

* ``occupied`` - Occupancy state (true = occupied, false = unoccupied)

This function will return ``true`` if successful, ``false`` otherwise.

setSensorType
^^^^^^^^^^^^^

Sets the sensor type.

.. code-block:: arduino

    bool setSensorType(uint8_t sensor_type);

* ``sensor_type`` - Sensor type identifier (see esp_zb_zcl_occupancy_sensing_occupancy_sensor_type_t)

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current occupancy state.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Occupancy Sensor Implementation
*******************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Occupancy_Sensor/Zigbee_Occupancy_Sensor.ino
    :language: arduino
