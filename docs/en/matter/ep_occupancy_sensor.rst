#####################
MatterOccupancySensor
#####################

About
-----

The ``MatterOccupancySensor`` class provides an occupancy sensor endpoint for Matter networks. This endpoint implements the Matter occupancy sensing standard for detecting occupied/unoccupied states (e.g., motion sensors, PIR sensors).

**Features:**
* Occupancy state reporting (occupied/unoccupied)
* Multiple sensor type support (PIR, Ultrasonic, Physical Contact)
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

Occupancy Sensor
****************

.. literalinclude:: ../../../libraries/Matter/examples/MatterOccupancySensor/MatterOccupancySensor.ino
    :language: arduino

