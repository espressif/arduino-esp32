###############
MatterOnOffLight
###############

About
-----

The ``MatterOnOffLight`` class provides a simple on/off light endpoint for Matter networks. This endpoint implements the Matter lighting standard for basic light control without dimming or color features.

**Features:**
* Simple on/off control
* State persistence support
* Callback support for state changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Simple smart lights
* On/off switches
* Basic lighting control
* Smart home automation

API Reference
-------------

Constructor
***********

MatterOnOffLight
^^^^^^^^^^^^^^^^

Creates a new Matter on/off light endpoint.

.. code-block:: arduino

    MatterOnOffLight();

Initialization
**************

begin
^^^^^

Initializes the Matter on/off light endpoint with an optional initial state.

.. code-block:: arduino

    bool begin(bool initialState = false);

* ``initialState`` - Initial on/off state (``true`` = on, ``false`` = off, default: ``false``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter light events. This will just stop processing Light Matter events but won't remove the endpoint.

.. code-block:: arduino

    void end();

On/Off Control
**************

setOnOff
^^^^^^^^

Sets the on/off state of the light.

.. code-block:: arduino

    bool setOnOff(bool newState);

* ``newState`` - New state (``true`` = on, ``false`` = off)

This function will return ``true`` if successful, ``false`` otherwise.

getOnOff
^^^^^^^^

Gets the current on/off state of the light.

.. code-block:: arduino

    bool getOnOff();

This function will return ``true`` if the light is on, ``false`` if off.

toggle
^^^^^^

Toggles the on/off state of the light.

.. code-block:: arduino

    bool toggle();

This function will return ``true`` if successful, ``false`` otherwise.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current on/off state of the light.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (myLight) {
        Serial.println("Light is on");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Turns the light on or off.

.. code-block:: arduino

    void operator=(bool state);

Example:

.. code-block:: arduino

    myLight = true;   // Turn light on
    myLight = false;  // Turn light off

Event Handling
**************

onChange
^^^^^^^^

Sets a callback function to be called when the light state changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

* ``onChangeCB`` - Function to call when state changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState);

* ``newState`` - New on/off state (``true`` = on, ``false`` = off)

The callback should return ``true`` if the change was handled successfully.

onChangeOnOff
^^^^^^^^^^^^^

Sets a callback function to be called when the on/off state changes (same as ``onChange``).

.. code-block:: arduino

    void onChangeOnOff(EndPointCB onChangeCB);

* ``onChangeCB`` - Function to call when state changes

updateAccessory
^^^^^^^^^^^^^^^

Updates the state of the light using the current Matter Light internal state. It is necessary to set a user callback function using ``onChange()`` to handle the physical light state.

.. code-block:: arduino

    void updateAccessory();

This function will call the registered callback with the current state.

Example
-------

Basic On/Off Light
******************

.. literalinclude:: ../../../libraries/Matter/examples/MatterOnOffLight/MatterOnOffLight.ino
    :language: arduino

