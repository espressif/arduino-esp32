###########################
ZigbeeElectricalMeasurement
###########################

About
-----

The ``ZigbeeElectricalMeasurement`` class provides an endpoint for electrical measurement devices in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for power monitoring and electrical measurement.

**Features:**
* AC and DC electrical measurements
* Voltage, current, and power monitoring
* Power factor measurement
* Multi-phase support
* Configurable measurement ranges
* Automatic reporting capabilities
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Smart power monitoring
* Energy monitoring systems
* Solar panel monitoring
* Battery monitoring systems
* Electrical load monitoring

Common API
----------

Constructor
***********

ZigbeeElectricalMeasurement
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee electrical measurement endpoint.

.. code-block:: arduino

    ZigbeeElectricalMeasurement(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

DC Electrical Measurement
*************************

addDCMeasurement
^^^^^^^^^^^^^^^^

Adds a DC measurement type to the endpoint.

.. code-block:: arduino

    bool addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);

* ``measurement_type`` - DC measurement type constant (ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_DC_MEASUREMENT_TYPE_POWER)

This function will return ``true`` if successful, ``false`` otherwise.

setDCMeasurement
^^^^^^^^^^^^^^^^

Sets the DC measurement value for a specific measurement type.

.. code-block:: arduino

    bool setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t value);

* ``measurement_type`` - DC measurement type constant (ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_DC_MEASUREMENT_TYPE_POWER)
* ``value`` - Measurement value

This function will return ``true`` if successful, ``false`` otherwise.

setDCMinMaxValue
^^^^^^^^^^^^^^^^

Sets the minimum and maximum values for a DC measurement type.

.. code-block:: arduino

    bool setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t min, int16_t max);

* ``measurement_type`` - DC measurement type constant (ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_DC_MEASUREMENT_TYPE_POWER)
* ``min`` - Minimum value
* ``max`` - Maximum value

This function will return ``true`` if successful, ``false`` otherwise.

setDCMultiplierDivisor
^^^^^^^^^^^^^^^^^^^^^^

Sets the multiplier and divisor for scaling DC measurements.

.. code-block:: arduino

    bool setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);

* ``measurement_type`` - DC measurement type constant (ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_DC_MEASUREMENT_TYPE_POWER)
* ``multiplier`` - Multiplier value
* ``divisor`` - Divisor value

This function will return ``true`` if successful, ``false`` otherwise.

setDCReporting
^^^^^^^^^^^^^^

Sets the reporting configuration for DC measurements.

.. code-block:: arduino

    bool setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t min_interval, uint16_t max_interval, int16_t delta);

* ``measurement_type`` - DC measurement type constant (ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_DC_MEASUREMENT_TYPE_POWER)
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report

This function will return ``true`` if successful, ``false`` otherwise.

reportDC
^^^^^^^^

Manually reports a DC measurement value.

.. code-block:: arduino

    bool reportDC(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);

* ``measurement_type`` - DC measurement type constant (ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_DC_MEASUREMENT_TYPE_POWER)

This function will return ``true`` if successful, ``false`` otherwise.

AC Electrical Measurement
*************************

addACMeasurement
^^^^^^^^^^^^^^^^

Adds an AC measurement type for a specific phase.

.. code-block:: arduino

    bool addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);

* ``measurement_type`` - AC measurement type constant (ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR, ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY)
* ``phase_type`` - Phase type constant (ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, ZIGBEE_AC_PHASE_TYPE_A, ZIGBEE_AC_PHASE_TYPE_B, ZIGBEE_AC_PHASE_TYPE_C)

This function will return ``true`` if successful, ``false`` otherwise.

setACMeasurement
^^^^^^^^^^^^^^^^

Sets the AC measurement value for a specific measurement type and phase.

.. code-block:: arduino

    bool setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t value);

* ``measurement_type`` - AC measurement type constant (ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR, ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY)
* ``phase_type`` - Phase type constant (ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, ZIGBEE_AC_PHASE_TYPE_A, ZIGBEE_AC_PHASE_TYPE_B, ZIGBEE_AC_PHASE_TYPE_C)
* ``value`` - Measurement value

This function will return ``true`` if successful, ``false`` otherwise.

setACMinMaxValue
^^^^^^^^^^^^^^^^

Sets the minimum and maximum values for an AC measurement type and phase.

.. code-block:: arduino

    bool setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t min, int32_t max);

* ``measurement_type`` - AC measurement type constant (ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR, ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY)
* ``phase_type`` - Phase type constant (ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, ZIGBEE_AC_PHASE_TYPE_A, ZIGBEE_AC_PHASE_TYPE_B, ZIGBEE_AC_PHASE_TYPE_C)
* ``min`` - Minimum value
* ``max`` - Maximum value

This function will return ``true`` if successful, ``false`` otherwise.

setACMultiplierDivisor
^^^^^^^^^^^^^^^^^^^^^^

Sets the multiplier and divisor for scaling AC measurements.

.. code-block:: arduino

    bool setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);

* ``measurement_type`` - AC measurement type constant (ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR, ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY)
* ``multiplier`` - Multiplier value
* ``divisor`` - Divisor value

This function will return ``true`` if successful, ``false`` otherwise.

setACPowerFactor
^^^^^^^^^^^^^^^^

Sets the power factor for a specific phase.

.. code-block:: arduino

    bool setACPowerFactor(ZIGBEE_AC_PHASE_TYPE phase_type, int8_t power_factor);

* ``phase_type`` - Phase type constant (ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, ZIGBEE_AC_PHASE_TYPE_A, ZIGBEE_AC_PHASE_TYPE_B, ZIGBEE_AC_PHASE_TYPE_C)
* ``power_factor`` - Power factor value (-100 to 100)

This function will return ``true`` if successful, ``false`` otherwise.

setACReporting
^^^^^^^^^^^^^^

Sets the reporting configuration for AC measurements.

.. code-block:: arduino

    bool setACReporting(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, uint16_t min_interval, uint16_t max_interval, int32_t delta);

* ``measurement_type`` - AC measurement type constant (ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR, ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY)
* ``phase_type`` - Phase type constant (ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, ZIGBEE_AC_PHASE_TYPE_A, ZIGBEE_AC_PHASE_TYPE_B, ZIGBEE_AC_PHASE_TYPE_C)
* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change required to trigger a report

This function will return ``true`` if successful, ``false`` otherwise.

reportAC
^^^^^^^^

Manually reports an AC measurement value.

.. code-block:: arduino

    bool reportAC(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);

* ``measurement_type`` - AC measurement type constant (ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE, ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT, ZIGBEE_AC_MEASUREMENT_TYPE_POWER, ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR, ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY)
* ``phase_type`` - Phase type constant (ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC, ZIGBEE_AC_PHASE_TYPE_A, ZIGBEE_AC_PHASE_TYPE_B, ZIGBEE_AC_PHASE_TYPE_C)

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

DC Electrical Measurement
*************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Electrical_DC_Sensor/Zigbee_Electrical_DC_Sensor.ino
    :language: arduino

AC Electrical Measurement
*************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Electrical_AC_Sensor_MultiPhase/Zigbee_Electrical_AC_Sensor_MultiPhase.ino
    :language: arduino
