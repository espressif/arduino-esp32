################
ZigbeeFanControl
################

About
-----

The ``ZigbeeFanControl`` class provides a Zigbee endpoint for controlling fan devices in a Home Automation (HA) network. This endpoint implements the Fan Control cluster and allows for controlling fan modes and sequences.

**Features:**
* Fan mode control (OFF, LOW, MEDIUM, HIGH, ON, AUTO, SMART)
* Fan mode sequence configuration
* State change callbacks
* Zigbee HA standard compliance

**Use Cases:**
* Smart ceiling fans
* HVAC system fans
* Exhaust fans
* Any device requiring fan speed control

API Reference
-------------

Constructor
***********

ZigbeeFanControl
^^^^^^^^^^^^^^^^

Creates a new Zigbee fan control endpoint.

.. code-block:: arduino

    ZigbeeFanControl(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Fan Mode Enums
**************

ZigbeeFanMode
^^^^^^^^^^^^^

Available fan modes for controlling the fan speed and operation.

.. code-block:: arduino

    enum ZigbeeFanMode {
      FAN_MODE_OFF,      // Fan is off
      FAN_MODE_LOW,      // Low speed
      FAN_MODE_MEDIUM,   // Medium speed
      FAN_MODE_HIGH,     // High speed
      FAN_MODE_ON,       // Fan is on (full speed)
      FAN_MODE_AUTO,     // Automatic mode
      FAN_MODE_SMART,    // Smart mode
    };

ZigbeeFanModeSequence
^^^^^^^^^^^^^^^^^^^^^

Available fan mode sequences that define which modes are available for the fan.

.. code-block:: arduino

    enum ZigbeeFanModeSequence {
      FAN_MODE_SEQUENCE_LOW_MED_HIGH,        // Low -> Medium -> High
      FAN_MODE_SEQUENCE_LOW_HIGH,            // Low -> High
      FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO,   // Low -> Medium -> High -> Auto
      FAN_MODE_SEQUENCE_LOW_HIGH_AUTO,       // Low -> High -> Auto
      FAN_MODE_SEQUENCE_ON_AUTO,             // On -> Auto
    };

API Methods
***********

setFanModeSequence
^^^^^^^^^^^^^^^^^^

Sets the fan mode sequence and initializes the fan control.

.. code-block:: arduino

    bool setFanModeSequence(ZigbeeFanModeSequence sequence);

* ``sequence`` - The fan mode sequence to set

This function will return ``true`` if successful, ``false`` otherwise.

getFanMode
^^^^^^^^^^

Gets the current fan mode.

.. code-block:: arduino

    ZigbeeFanMode getFanMode();

This function will return current fan mode.

getFanModeSequence
^^^^^^^^^^^^^^^^^^

Gets the current fan mode sequence.

.. code-block:: arduino

    ZigbeeFanModeSequence getFanModeSequence();

This function will return current fan mode sequence.

Event Handling
**************

onFanModeChange
^^^^^^^^^^^^^^^

Sets a callback function to be called when the fan mode changes.

.. code-block:: arduino

    void onFanModeChange(void (*callback)(ZigbeeFanMode mode));

* ``callback`` - Function to call when fan mode changes

Example
-------

Fan Control Implementation
**************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Fan_Control/Zigbee_Fan_Control.ino
    :language: arduino
