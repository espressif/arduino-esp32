#################
MatterOnOffPlugin
#################

About
-----

The ``MatterOnOffPlugin`` class provides an on/off plugin unit endpoint for Matter networks. This endpoint implements the Matter on/off plugin standard for controlling power outlets, relays, and other on/off devices.

**Features:**
* Simple on/off control
* State persistence support
* Callback support for state changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Smart power outlets
* Relay control
* Power switches
* Smart plugs
* On/off device control

API Reference
-------------

Constructor
***********

MatterOnOffPlugin
^^^^^^^^^^^^^^^^^

Creates a new Matter on/off plugin endpoint.

.. code-block:: arduino

    MatterOnOffPlugin();

Initialization
**************

begin
^^^^^

Initializes the Matter on/off plugin endpoint with an optional initial state.

.. code-block:: arduino

    bool begin(bool initialState = false);

* ``initialState`` - Initial on/off state (``true`` = on, ``false`` = off, default: ``false``)

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

Sets a callback function to be called when the plugin state changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

* ``onChangeCB`` - Function to call when state changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState);

* ``newState`` - New on/off state (``true`` = on, ``false`` = off)

onChangeOnOff
^^^^^^^^^^^^^

Sets a callback function to be called when the on/off state changes (same as ``onChange``).

.. code-block:: arduino

    void onChangeOnOff(EndPointCB onChangeCB);

updateAccessory
^^^^^^^^^^^^^^^

Updates the state of the plugin using the current Matter Plugin internal state.

.. code-block:: arduino

    void updateAccessory();

This function will call the registered callback with the current state.

Example
-------

On/Off Plugin
*************

.. literalinclude:: ../../../libraries/Matter/examples/MatterOnOffPlugin/MatterOnOffPlugin.ino
    :language: arduino

