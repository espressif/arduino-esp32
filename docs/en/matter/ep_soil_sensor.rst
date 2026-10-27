################
MatterSoilSensor
################

About
-----

The ``MatterSoilSensor`` class provides a soil sensor endpoint for Matter networks (device type ``0x0045``). This endpoint implements the Matter Soil Measurement cluster (``0x0430``) as a code-driven CHIP server object.

**Features:**

* Soil moisture reporting from 0–100%.
* Read-only measurement path via ``setSoilMoisture()`` (no sub-percent precision).
* Built on the ``esp_matter::endpoint::soil_sensor`` device-type wrapper with a code-driven Soil Measurement cluster.
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

Creates the endpoint (Descriptor, Identify, device type, Soil Measurement cluster placeholder). Call before ``Matter.begin()``. The overload ``begin(uint8_t initialSoilMoisturePercent)`` additionally seeds an initial whole-percent reading, which is cached until the live cluster is available.

.. code-block:: arduino

    bool begin();
    bool begin(uint8_t initialSoilMoisturePercent);

``begin()`` takes no initial moisture value. The measured attribute stays null until the first successful ``setSoilMoisture()``.

``begin(uint8_t)`` does the same and then seeds the moisture cache. ``initialSoilMoisturePercent`` is a whole number in ``[0..100]``. When this call happens before ``Matter.begin()``, the value is applied in ``onStackStarted()`` once the live cluster is registered.

Both return ``true`` if successful, ``false`` otherwise.

Typical usage:

.. code-block:: arduino

    SoilSensor.begin();
    Matter.begin();
    SoilSensor.setSoilMoisture(readingPercent);

The live ``SoilMeasurementCluster`` is constructed when the Matter stack starts. ``setSoilMoisture()`` may be called after the endpoint ``begin()`` even before ``Matter.begin()``: the value is cached and applied once the live cluster is registered.

end
^^^

Stops processing Matter soil sensor events.

.. code-block:: arduino

    void end();

Soil moisture
*************

setSoilMoisture
^^^^^^^^^^^^^^^

Sets the reported soil moisture (0–100%) after endpoint ``begin()``. Before ``Matter.begin()`` the value is cached and applied when the live cluster is registered; afterward it updates the live cluster directly.

.. code-block:: arduino

    bool setSoilMoisture(uint8_t soilMoisturePercent);

* ``soilMoisturePercent`` - Whole number in ``[0..100]``.

This function will return ``true`` if successful, ``false`` otherwise.

getSoilMoisture
^^^^^^^^^^^^^^^

Returns the cached soil moisture (0–100%). This is ``0`` until the first ``setSoilMoisture()``. The cluster attribute stays null until a publish succeeds.

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

* ``SoilMoistureMeasuredValue`` is not available through ``getAttributeVal()`` / ``updateAttributeVal()``; use ``setSoilMoisture()`` which calls ``SoilMeasurement::SetSoilMoistureMeasuredValue()`` under the CHIP stack lock. A repeated call with the same value is skipped only after that publish succeeds.
* Unlike ``MatterHumiditySensor``, there is no sub-percent precision.

Example
-------

Soil Sensor
***********

.. literalinclude:: ../../../libraries/Matter/examples/Sensors/MatterSoilSensor/MatterSoilSensor.ino
    :language: arduino
