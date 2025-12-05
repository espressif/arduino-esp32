####################
MatterDimmablePlugin
####################

About
-----

The ``MatterDimmablePlugin`` class provides a dimmable plugin unit endpoint for Matter networks. This endpoint implements the Matter dimmable plugin standard for controlling power outlets, relays, and other dimmable devices with power level control.

**Features:**
* On/off control
* Power level control (0-255)
* State persistence support
* Callback support for state and level changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Dimmable smart power outlets
* Variable power control
* Smart dimmer plugs
* Level-controlled device control

API Reference
-------------

Constructor
***********

MatterDimmablePlugin
^^^^^^^^^^^^^^^^^^^^

Creates a new Matter dimmable plugin endpoint.

.. code-block:: arduino

    MatterDimmablePlugin();

Initialization
**************

begin
^^^^^

Initializes the Matter dimmable plugin endpoint with optional initial state and level.

.. code-block:: arduino

    bool begin(bool initialState = false, uint8_t level = 64);

* ``initialState`` - Initial on/off state (``true`` = on, ``false`` = off, default: ``false``)
* ``level`` - Initial power level (0-255, default: 64 = 25%)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter plugin events.

.. code-block:: arduino

    void end();

On/Off Control
**************

setOnOff
^^^^^^^^

Sets the on/off state of the plugin.

.. code-block:: arduino

    bool setOnOff(bool newState);

* ``newState`` - New state (``true`` = on, ``false`` = off)

This function will return ``true`` if successful, ``false`` otherwise.

getOnOff
^^^^^^^^

Gets the current on/off state of the plugin.

.. code-block:: arduino

    bool getOnOff();

This function will return ``true`` if the plugin is on, ``false`` if off.

toggle
^^^^^^

Toggles the on/off state of the plugin.

.. code-block:: arduino

    bool toggle();

This function will return ``true`` if successful, ``false`` otherwise.

Level Control
**************

setLevel
^^^^^^^^

Sets the power level of the plugin.

.. code-block:: arduino

    bool setLevel(uint8_t newLevel);

* ``newLevel`` - New power level (0-255, where 0 = off, 255 = maximum level)

This function will return ``true`` if successful, ``false`` otherwise.

getLevel
^^^^^^^^

Gets the current power level of the plugin.

.. code-block:: arduino

    uint8_t getLevel();

This function will return the power level (0-255).

Constants
*********

MAX_LEVEL
^^^^^^^^^^

Maximum power level value constant.

.. code-block:: arduino

    static const uint8_t MAX_LEVEL = 255;

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current on/off state of the plugin.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (myPlugin) {
        Serial.println("Plugin is on");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Turns the plugin on or off.

.. code-block:: arduino

    void operator=(bool state);

Example:

.. code-block:: arduino

    myPlugin = true;   // Turn plugin on
    myPlugin = false;  // Turn plugin off

Event Handling
**************

onChange
^^^^^^^^

Sets a callback function to be called when any parameter changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

* ``onChangeCB`` - Function to call when state changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState, uint8_t newLevel);

* ``newState`` - New on/off state (``true`` = on, ``false`` = off)
* ``newLevel`` - New power level (0-255)

onChangeOnOff
^^^^^^^^^^^^^

Sets a callback function to be called when the on/off state changes.

.. code-block:: arduino

    void onChangeOnOff(EndPointOnOffCB onChangeCB);

* ``onChangeCB`` - Function to call when on/off state changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState);

* ``newState`` - New on/off state (``true`` = on, ``false`` = off)

onChangeLevel
^^^^^^^^^^^^^

Sets a callback function to be called when the power level changes.

.. code-block:: arduino

    void onChangeLevel(EndPointLevelCB onChangeCB);

* ``onChangeCB`` - Function to call when level changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(uint8_t newLevel);

* ``newLevel`` - New power level (0-255)

updateAccessory
^^^^^^^^^^^^^^^

Updates the state of the plugin using the current Matter Plugin internal state.

.. code-block:: arduino

    void updateAccessory();

This function will call the registered callback with the current state.

Example
-------

Dimmable Plugin
***************

.. literalinclude:: ../../../libraries/Matter/examples/MatterDimmablePlugin/MatterDimmablePlugin.ino
    :language: arduino

