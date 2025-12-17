#####################
MatterOccupancySensor
#####################

About
-----

The ``MatterOccupancySensor`` class provides an occupancy sensor endpoint for Matter networks. This endpoint implements the Matter occupancy sensing standard for detecting occupied/unoccupied states (e.g., motion sensors, PIR sensors).

**Features:**
* Occupancy state reporting (occupied/unoccupied)
* Multiple sensor type support (PIR, Ultrasonic, Physical Contact)
* HoldTime attribute for configuring how long the sensor holds the "occupied" state
* HoldTimeLimits (min, max, default) for validation and controller guidance
* HoldTime change callback for real-time updates from Matter controllers
* Simple boolean state
* Read-only sensor (no control functionality)
* Automatic state updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Motion sensors (PIR)
* Occupancy detection
* Security systems
* Smart lighting automation
* Energy management (turn off lights when unoccupied)

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

* ``_occupancyState`` - Initial occupancy state (``true`` = occupied, ``false`` = unoccupied, default: ``false``)
* ``_occupancySensorType`` - Sensor type (default: ``OCCUPANCY_SENSOR_TYPE_PIR``)

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

* ``OCCUPANCY_SENSOR_TYPE_PIR`` - Passive Infrared (PIR) sensor
* ``OCCUPANCY_SENSOR_TYPE_ULTRASONIC`` - Ultrasonic sensor
* ``OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC`` - Combined PIR and Ultrasonic
* ``OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT`` - Physical contact sensor

Occupancy State Control
***********************

setOccupancy
^^^^^^^^^^^^

Sets the occupancy state.

.. code-block:: arduino

    bool setOccupancy(bool _occupancyState);

* ``_occupancyState`` - Occupancy state (``true`` = occupied, ``false`` = unoccupied)

This function will return ``true`` if successful, ``false`` otherwise.

getOccupancy
^^^^^^^^^^^^

Gets the current occupancy state.

.. code-block:: arduino

    bool getOccupancy();

This function will return ``true`` if occupied, ``false`` if unoccupied.

HoldTime Control
****************

setHoldTime
^^^^^^^^^^^

Sets the HoldTime value (in seconds). The HoldTime determines how long the sensor maintains the "occupied" state after the last detection.

.. code-block:: arduino

    bool setHoldTime(uint16_t _holdTime_seconds);

* ``_holdTime_seconds`` - HoldTime value in seconds

**Important:** This function must be called after ``Matter.begin()`` has been called, as it requires the Matter event loop to be running.

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

* ``_holdTimeMin_seconds`` - Minimum HoldTime value in seconds
* ``_holdTimeMax_seconds`` - Maximum HoldTime value in seconds
* ``_holdTimeDefault_seconds`` - Default/recommended HoldTime value in seconds (informational metadata for controllers)

**Important:** 
* This function must be called after ``Matter.begin()`` has been called, as it requires the Matter event loop to be running.
* The ``holdTimeDefault_seconds`` parameter is informational metadata for Matter controllers (recommended default value). It does NOT automatically set the HoldTime attribute - use ``setHoldTime()`` to set the actual value.
* If the current HoldTime value is outside the new limits, it will be automatically adjusted to the nearest limit (minimum or maximum).

This function will return ``true`` if successful, ``false`` otherwise.

onHoldTimeChange
^^^^^^^^^^^^^^^^

Sets a callback function that will be called when the HoldTime value is changed by a Matter Controller.

.. code-block:: arduino

    void onHoldTimeChange(HoldTimeChangeCB onHoldTimeChangeCB);

* ``onHoldTimeChangeCB`` - Callback function of type ``HoldTimeChangeCB``

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

.. literalinclude:: ../../../libraries/Matter/examples/MatterOccupancySensor/MatterOccupancySensor.ino
    :language: arduino

Occupancy Sensor with HoldTime
*******************************

For an example that demonstrates HoldTime functionality, see:

.. literalinclude:: ../../../libraries/Matter/examples/MatterOccupancyWithHoldTime/MatterOccupancyWithHoldTime.ino
    :language: arduino

This example shows:
* How to configure HoldTimeLimits after ``Matter.begin()``
* How to set and persist HoldTime values
* How to use the ``onHoldTimeChange()`` callback
* How to implement HoldTime expiration logic in sensor simulation
