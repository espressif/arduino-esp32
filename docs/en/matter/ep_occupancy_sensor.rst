#####################
MatterOccupancySensor
#####################

About
-----

The ``MatterOccupancySensor`` class provides an occupancy sensor endpoint for Matter networks. This endpoint implements the Matter occupancy sensing standard for detecting occupied/unoccupied states (e.g., motion sensors, PIR sensors).

**Features:**
* Occupancy state reporting (occupied/unoccupied).
* Multiple sensor type support (PIR, Ultrasonic, Physical Contact).
* Optional HoldTime (seconds). Off unless ``setHoldTime()`` / ``setHoldTimeLimits()`` is called after the sensor ``begin()`` and before ``Matter.begin()``.
* HoldTimeLimits (min, max, default) for controller validation when HoldTime is enabled.
* HoldTime change callback for real-time updates from Matter controllers.
* Simple boolean state.
* Read-only sensor (no control functionality).
* Automatic state updates.
* Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
* Matter standard compliance.

**Use Cases:**
* Motion sensors (PIR).
* Occupancy detection.
* Security systems.
* Smart lighting automation.
* Energy management (turn off lights when unoccupied).

API Reference
-------------

Constructor
***********

MatterOccupancySensor
^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter occupancy sensor endpoint.

.. code-block:: arduino

    MatterOccupancySensor();

Initialization
**************

begin
^^^^^

Initializes the Matter occupancy sensor endpoint with an initial occupancy state and sensor type.

.. code-block:: arduino

    bool begin(bool _occupancyState = false, OccupancySensorType_t _occupancySensorType = OCCUPANCY_SENSOR_TYPE_PIR);

* ``_occupancyState`` - Initial occupancy state (``true`` = occupied, ``false`` = unoccupied, default: ``false``).
* ``_occupancySensorType`` - Sensor type (default: ``OCCUPANCY_SENSOR_TYPE_PIR``).

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter occupancy sensor events.

.. code-block:: arduino

    void end();

Sensor Types
************

OccupancySensorType_t
^^^^^^^^^^^^^^^^^^^^^^

Occupancy sensor type enumeration:

* ``OCCUPANCY_SENSOR_TYPE_PIR`` - Passive Infrared (PIR) sensor.
* ``OCCUPANCY_SENSOR_TYPE_ULTRASONIC`` - Ultrasonic sensor.
* ``OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC`` - Combined PIR and Ultrasonic.
* ``OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT`` - Physical contact sensor.

getOccupancySensorType
^^^^^^^^^^^^^^^^^^^^^^

Returns the sensor type passed to ``begin()``. Occupancy Sensing features at create time set the cluster FeatureMap (no occupancy HAL).

.. code-block:: arduino

    OccupancySensorType_t getOccupancySensorType();

Occupancy State Control
***********************

setOccupancy
^^^^^^^^^^^^

Sets the occupancy state.

.. code-block:: arduino

    bool setOccupancy(bool _occupancyState);

* ``_occupancyState`` - Occupancy state (``true`` = occupied, ``false`` = unoccupied).

With HoldTime enabled, ``setOccupancy(false)`` starts the cluster hold timer instead of going vacant immediately. A second ``setOccupancy(false)`` would restart that timer, so unchanged requests are ignored. ``getOccupancy()`` is the last request; ``isOccupied()`` is the Occupancy attribute the hub reads.

This function will return ``true`` if successful, ``false`` otherwise.

getOccupancy
^^^^^^^^^^^^

Returns the last occupancy value requested by the sketch, not the held cluster state.

.. code-block:: arduino

    bool getOccupancy();

This function will return ``true`` if the sketch last requested occupied, ``false`` if vacant.

isOccupied
^^^^^^^^^^

Returns the Occupancy attribute the hub reads, including HoldTime.

.. code-block:: arduino

    bool isOccupied();

This function will return ``true`` if the Occupancy attribute is occupied (including HoldTime), ``false`` if vacant.

HoldTime Control
****************

setHoldTime
^^^^^^^^^^^

Sets the HoldTime attribute (in seconds). Call after the sensor ``begin()`` and **before** ``Matter.begin()`` to create the HoldTime attributes. After ``Matter.begin()`` it only updates a cluster that already has HoldTime enabled. ``0`` is not allowed.

.. code-block:: arduino

    bool setHoldTime(uint16_t _holdTime_seconds);

* ``_holdTime_seconds`` - HoldTime value in seconds (at least 1).

When limits are configured (``holdTimeMax > 0``), the value must fall in that range.

This function will return ``true`` if successful, ``false`` otherwise.

getHoldTime
^^^^^^^^^^^

Gets the current HoldTime value (in seconds).

.. code-block:: arduino

    uint16_t getHoldTime();

This function will return the current HoldTime value in seconds.

setHoldTimeLimits
^^^^^^^^^^^^^^^^^

Sets the HoldTime limits (minimum, maximum, and default values). These limits define the valid range for HoldTime values and provide metadata for Matter controllers.

.. code-block:: arduino

    bool setHoldTimeLimits(uint16_t _holdTimeMin_seconds, uint16_t _holdTimeMax_seconds, uint16_t _holdTimeDefault_seconds);

* ``_holdTimeMin_seconds`` - Minimum HoldTime value in seconds (CHIP coerces values below 1 up to 1).
* ``_holdTimeMax_seconds`` - Maximum HoldTime value in seconds (CHIP coerces values below 10 up to at least 10).
* ``_holdTimeDefault_seconds`` - Default/recommended HoldTime value in seconds (informational metadata for controllers; clamped into the min/max range).

**Important:**
* Call after the sensor ``begin()`` and **before** ``Matter.begin()`` to enable HoldTime. After ``Matter.begin()`` it only updates a cluster that already has HoldTime enabled.
* The ``holdTimeDefault_seconds`` parameter is informational metadata for Matter controllers (recommended default value). It does NOT automatically set the HoldTime attribute - use ``setHoldTime()`` to set the actual value.
* If the current HoldTime value is outside the new limits, it will be automatically adjusted to the nearest limit (minimum or maximum).

This function will return ``true`` if successful, ``false`` otherwise.

onHoldTimeChange
^^^^^^^^^^^^^^^^

Sets a callback function that will be called when the HoldTime value is changed by a Matter Controller.

.. code-block:: arduino

    void onHoldTimeChange(HoldTimeChangeCB onHoldTimeChangeCB);

* ``onHoldTimeChangeCB`` - Callback function of type ``HoldTimeChangeCB``.

The callback function signature is:

.. code-block:: arduino

    using HoldTimeChangeCB = std::function<bool(uint16_t holdTime_seconds)>;

The callback receives the new HoldTime value and can return ``true`` to accept the change or ``false`` to reject it.

Example:

.. code-block:: arduino

    OccupancySensor.onHoldTimeChange([](uint16_t holdTime_seconds) -> bool {
        Serial.printf("HoldTime changed to %u seconds\n", holdTime_seconds);
        return true;  // Accept the change
    });

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current occupancy state.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (mySensor) {
        Serial.println("Room is occupied");
    } else {
        Serial.println("Room is unoccupied");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the occupancy state.

.. code-block:: arduino

    void operator=(bool _occupancyState);

Example:

.. code-block:: arduino

    mySensor = true;   // Set to occupied
    mySensor = false;  // Set to unoccupied

Example
-------

Basic Occupancy Sensor
**********************

.. literalinclude:: ../../../libraries/Matter/examples/Sensors/MatterOccupancySensor/MatterOccupancySensor.ino
    :language: arduino

Occupancy Sensor with HoldTime
*******************************

For an example that demonstrates HoldTime functionality, see:

.. literalinclude:: ../../../libraries/Matter/examples/Sensors/MatterOccupancyWithHoldTime/MatterOccupancyWithHoldTime.ino
    :language: arduino

This example shows:
* How to configure HoldTimeLimits and HoldTime after the sensor ``begin()`` and before ``Matter.begin()``.
* How to persist HoldTime values.
* How to use the ``onHoldTimeChange()`` callback.
* How to report a raw motion pulse and let CHIP HoldTime hold Occupancy for the hub.
