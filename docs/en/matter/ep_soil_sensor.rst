##################
MatterSoilSensor
##################

About
-----

The ``MatterSoilSensor`` class provides a soil sensor endpoint for Matter networks (device type ``0x0045``). This endpoint implements the Matter Soil Measurement cluster (``0x0430``) as a code-driven CHIP server object.

**Features:**

* Soil moisture reporting as whole percent (0–100).
* Read-only measurement path via ``setSoilMoisture()`` (no sub-percent precision).
* Code-driven cluster registration (no ``esp_matter::endpoint::soil_sensor`` wrapper yet).
* Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
* Matter standard compliance.

**Use Cases:**

* Garden and greenhouse monitoring.
* Irrigation automation triggers.
* Potted plant soil moisture logging.

API Reference
-------------

Constructor
***********

MatterSoilSensor
^^^^^^^^^^^^^^^^

Creates a new Matter soil sensor endpoint.

.. code-block:: arduino

    MatterSoilSensor();

Initialization
**************

begin
^^^^^

Creates the endpoint (Descriptor, Identify, device type, Soil Measurement cluster placeholder). Call before ``Matter.begin()``. Takes no initial moisture value.

.. code-block:: arduino

    bool begin();

This function will return ``true`` if successful, ``false`` otherwise.

Typical usage:

.. code-block:: arduino

    SoilSensor.begin();
    Matter.begin();
    SoilSensor.setSoilMoisture(readingPercent);

The live ``SoilMeasurementCluster`` is constructed when the Matter stack starts. Call ``setSoilMoisture()`` after ``Matter.begin()``.

end
^^^

Stops processing Matter soil sensor events.

.. code-block:: arduino

    void end();

Soil moisture
*************

setSoilMoisture
^^^^^^^^^^^^^^^

Sets the reported soil moisture percent. Requires ``Matter.begin()`` so the cluster is registered.

.. code-block:: arduino

    bool setSoilMoisture(uint8_t soilMoisturePercent);

* ``soilMoisturePercent`` - Whole percent in ``[0..100]``.

This function will return ``true`` if successful, ``false`` otherwise.

getSoilMoisture
^^^^^^^^^^^^^^^

Returns the last value successfully reported through ``setSoilMoisture()``.

.. code-block:: arduino

    uint8_t getSoilMoisture();

Operators
*********

uint8_t operators
^^^^^^^^^^^^^^^^^

Assignment and conversion operators call ``setSoilMoisture()`` / ``getSoilMoisture()``.

.. code-block:: arduino

    void operator=(uint8_t soilMoisturePercent);
    operator uint8_t();

Implementation notes
********************

* ``SoilMoistureMeasuredValue`` is not available through ``getAttributeVal()`` / ``updateAttributeVal()``; use ``setSoilMoisture()`` which calls ``SoilMeasurementCluster::SetSoilMoistureMeasuredValue()`` under the CHIP stack lock.
* Unlike ``MatterHumiditySensor``, there is no 1/100th percent precision.

Example
-------

Soil Sensor
***********

.. literalinclude:: ../../../libraries/Matter/examples/Sensors/MatterSoilSensor/MatterSoilSensor.ino
    :language: arduino
