###################
MatterDimmableLight
###################

About
-----

The ``MatterDimmableLight`` class provides a dimmable light endpoint for Matter networks. This endpoint implements the Matter lighting standard for lights with brightness control.

**Features:**
* On/off control
* Brightness level control (0-255)
* State persistence support
* Callback support for state and brightness changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Dimmable smart lights
* Brightness control switches
* Smart home lighting automation
* Variable brightness lighting

API Reference
-------------

Constructor
***********

MatterDimmableLight
^^^^^^^^^^^^^^^^^^^

Creates a new Matter dimmable light endpoint.

.. code-block:: arduino

    MatterDimmableLight();

Initialization
**************

begin
^^^^^

Initializes the Matter dimmable light endpoint with optional initial state and brightness.

.. code-block:: arduino

    bool begin(bool initialState = false, uint8_t brightness = 64);

* ``initialState`` - Initial on/off state (``true`` = on, ``false`` = off, default: ``false``)
* ``brightness`` - Initial brightness level (0-255, default: 64 = 25%)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter light events.

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

Brightness Control
******************

setBrightness
^^^^^^^^^^^^^

Sets the brightness level of the light.

.. code-block:: arduino

    bool setBrightness(uint8_t newBrightness);

* ``newBrightness`` - New brightness level (0-255, where 0 = off, 255 = maximum brightness)

This function will return ``true`` if successful, ``false`` otherwise.

getBrightness
^^^^^^^^^^^^^

Gets the current brightness level of the light.

.. code-block:: arduino

    uint8_t getBrightness();

This function will return the brightness level (0-255).

Constants
*********

MAX_BRIGHTNESS
^^^^^^^^^^^^^^

Maximum brightness value constant.

.. code-block:: arduino

    static const uint8_t MAX_BRIGHTNESS = 255;

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current on/off state of the light.

.. code-block:: arduino

    operator bool();

Assignment operator
^^^^^^^^^^^^^^^^^^^

Turns the light on or off.

.. code-block:: arduino

    void operator=(bool state);

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

    bool onChangeCallback(bool newState, uint8_t newBrightness);

* ``newState`` - New on/off state
* ``newBrightness`` - New brightness level (0-255)

onChangeOnOff
^^^^^^^^^^^^^

Sets a callback function to be called when the on/off state changes.

.. code-block:: arduino

    void onChangeOnOff(EndPointOnOffCB onChangeCB);

* ``onChangeCB`` - Function to call when on/off state changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState);

onChangeBrightness
^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when the brightness level changes.

.. code-block:: arduino

    void onChangeBrightness(EndPointBrightnessCB onChangeCB);

* ``onChangeCB`` - Function to call when brightness changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(uint8_t newBrightness);

updateAccessory
^^^^^^^^^^^^^^^

Updates the state of the light using the current Matter Light internal state.

.. code-block:: arduino

    void updateAccessory();

Example
-------

Dimmable Light
**************

.. literalinclude:: ../../../libraries/Matter/examples/MatterDimmableLight/MatterDimmableLight.ino
    :language: arduino

