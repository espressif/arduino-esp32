################
ZigbeePM25Sensor
################

About
-----

The ``ZigbeePM25Sensor`` class provides a PM2.5 air quality sensor endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for particulate matter measurement devices.

**Features:**
* PM2.5 concentration measurement in μg/m³
* Configurable measurement range
* Tolerance and reporting configuration
* Automatic reporting capabilities

API Reference
-------------

Constructor
***********

ZigbeePM25Sensor
^^^^^^^^^^^^^^^^

Creates a new Zigbee PM2.5 sensor endpoint.

.. code-block:: arduino

    ZigbeePM25Sensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setPM25
^^^^^^^

Sets the PM2.5 concentration measurement value.

.. code-block:: arduino

    bool setPM25(float pm25);

* ``pm25`` - PM2.5 concentration value in 0.1 μg/m³

This function will return ``true`` if successful, ``false`` otherwise.

setMinMaxValue
^^^^^^^^^^^^^^

Sets the minimum and maximum measurement values.

.. code-block:: arduino

    bool setMinMaxValue(float min, float max);

* ``min`` - Minimum PM2.5 concentration value in 0.1 μg/m³
* ``max`` - Maximum PM2.5 concentration value in 0.1 μg/m³

This function will return ``true`` if successful, ``false`` otherwise.

setTolerance
^^^^^^^^^^^^

Sets the tolerance value for measurements.

.. code-block:: arduino

    bool setTolerance(float tolerance);

* ``tolerance`` - Tolerance value in 0.1 μg/m³

This function will return ``true`` if successful, ``false`` otherwise.

setReporting
^^^^^^^^^^^^

Sets the reporting configuration for PM2.5 measurements.

.. code-block:: arduino

    bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report in 0.1 μg/m³

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current PM2.5 concentration value.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

PM2.5 Sensor Implementation
***************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_PM25_Sensor/Zigbee_PM25_Sensor.ino
    :language: arduino
