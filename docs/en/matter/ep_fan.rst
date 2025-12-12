#########
MatterFan
#########

About
-----

The ``MatterFan`` class provides a fan endpoint for Matter networks with speed and mode control. This endpoint implements the Matter fan control standard.

**Features:**
* On/off control
* Fan speed control (0-100%)
* Fan mode control (OFF, LOW, MEDIUM, HIGH, ON, AUTO, SMART)
* Fan mode sequence configuration
* Callback support for state, speed, and mode changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Smart ceiling fans
* Exhaust fans
* Ventilation fans
* Fan speed controllers
* HVAC fan control

API Reference
-------------

Constructor
***********

MatterFan
^^^^^^^^^

Creates a new Matter fan endpoint.

.. code-block:: arduino

    MatterFan();

Initialization
**************

begin
^^^^^

Initializes the Matter fan endpoint with optional initial speed, mode, and mode sequence.

.. code-block:: arduino

    bool begin(uint8_t percent = 0, FanMode_t fanMode = FAN_MODE_OFF, FanModeSequence_t fanModeSeq = FAN_MODE_SEQ_OFF_HIGH);

* ``percent`` - Initial speed percentage (0-100, default: 0)
* ``fanMode`` - Initial fan mode (default: ``FAN_MODE_OFF``)
* ``fanModeSeq`` - Fan mode sequence configuration (default: ``FAN_MODE_SEQ_OFF_HIGH``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter fan events.

.. code-block:: arduino

    void end();

Constants
*********

MAX_SPEED
^^^^^^^^^

Maximum speed value (100%).

.. code-block:: arduino

    static const uint8_t MAX_SPEED = 100;

MIN_SPEED
^^^^^^^^^

Minimum speed value (1%).

.. code-block:: arduino

    static const uint8_t MIN_SPEED = 1;

OFF_SPEED
^^^^^^^^^

Speed value when fan is off (0%).

.. code-block:: arduino

    static const uint8_t OFF_SPEED = 0;

Fan Modes
*********

FanMode_t
^^^^^^^^^

Fan mode enumeration:

* ``FAN_MODE_OFF`` - Fan is off
* ``FAN_MODE_LOW`` - Low speed
* ``FAN_MODE_MEDIUM`` - Medium speed
* ``FAN_MODE_HIGH`` - High speed
* ``FAN_MODE_ON`` - Fan is on
* ``FAN_MODE_AUTO`` - Auto mode
* ``FAN_MODE_SMART`` - Smart mode

Fan Mode Sequences
******************

FanModeSequence_t
^^^^^^^^^^^^^^^^^

Fan mode sequence enumeration:

* ``FAN_MODE_SEQ_OFF_LOW_MED_HIGH`` - OFF, LOW, MEDIUM, HIGH
* ``FAN_MODE_SEQ_OFF_LOW_HIGH`` - OFF, LOW, HIGH
* ``FAN_MODE_SEQ_OFF_LOW_MED_HIGH_AUTO`` - OFF, LOW, MEDIUM, HIGH, AUTO
* ``FAN_MODE_SEQ_OFF_LOW_HIGH_AUTO`` - OFF, LOW, HIGH, AUTO
* ``FAN_MODE_SEQ_OFF_HIGH_AUTO`` - OFF, HIGH, AUTO
* ``FAN_MODE_SEQ_OFF_HIGH`` - OFF, HIGH

On/Off Control
**************

setOnOff
^^^^^^^^

Sets the on/off state of the fan.

.. code-block:: arduino

    bool setOnOff(bool newState, bool performUpdate = true);

* ``newState`` - New state (``true`` = on, ``false`` = off)
* ``performUpdate`` - Perform update after setting (default: ``true``)

getOnOff
^^^^^^^^

Gets the current on/off state.

.. code-block:: arduino

    bool getOnOff();

toggle
^^^^^^

Toggles the on/off state.

.. code-block:: arduino

    bool toggle(bool performUpdate = true);

Speed Control
*************

setSpeedPercent
^^^^^^^^^^^^^^^

Sets the fan speed percentage.

.. code-block:: arduino

    bool setSpeedPercent(uint8_t newPercent, bool performUpdate = true);

* ``newPercent`` - Speed percentage (0-100)
* ``performUpdate`` - Perform update after setting (default: ``true``)

getSpeedPercent
^^^^^^^^^^^^^^^

Gets the current speed percentage.

.. code-block:: arduino

    uint8_t getSpeedPercent();

Mode Control
************

setMode
^^^^^^^

Sets the fan mode.

.. code-block:: arduino

    bool setMode(FanMode_t newMode, bool performUpdate = true);

* ``newMode`` - Fan mode to set
* ``performUpdate`` - Perform update after setting (default: ``true``)

getMode
^^^^^^^

Gets the current fan mode.

.. code-block:: arduino

    FanMode_t getMode();

getFanModeString
^^^^^^^^^^^^^^^^

Gets a friendly string for the fan mode.

.. code-block:: arduino

    static const char *getFanModeString(uint8_t mode);

Event Handling
**************

onChange
^^^^^^^^

Sets a callback for when any parameter changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(FanMode_t newMode, uint8_t newPercent);

onChangeMode
^^^^^^^^^^^^

Sets a callback for mode changes.

.. code-block:: arduino

    void onChangeMode(EndPointModeCB onChangeCB);

onChangeSpeedPercent
^^^^^^^^^^^^^^^^^^^^

Sets a callback for speed changes.

.. code-block:: arduino

    void onChangeSpeedPercent(EndPointSpeedCB onChangeCB);

updateAccessory
^^^^^^^^^^^^^^^

Updates the physical fan state using current Matter internal state.

.. code-block:: arduino

    void updateAccessory();

Operators
*********

uint8_t operator
^^^^^^^^^^^^^^^^

Returns the current speed percentage.

.. code-block:: arduino

    operator uint8_t();

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the speed percentage.

.. code-block:: arduino

    void operator=(uint8_t speedPercent);

Example
-------

Fan Control
***********

.. literalinclude:: ../../../libraries/Matter/examples/MatterFan/MatterFan.ino
    :language: arduino
