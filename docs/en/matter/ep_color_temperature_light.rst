############################
MatterColorTemperatureLight
############################

About
-----

The ``MatterColorTemperatureLight`` class provides a color temperature light endpoint for Matter networks with brightness and color temperature control. This endpoint implements the Matter lighting standard for lights that support color temperature adjustment (warm white to cool white).

**Features:**
* On/off control
* Brightness level control (0-255)
* Color temperature control (100-500 mireds)
* State persistence support
* Callback support for state, brightness, and temperature changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Tunable white lights
* Color temperature adjustable lights
* Smart lighting with warm/cool control
* Circadian lighting
* Smart home lighting automation

API Reference
-------------

Constructor
***********

MatterColorTemperatureLight
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter color temperature light endpoint.

.. code-block:: arduino

    MatterColorTemperatureLight();

Initialization
**************

begin
^^^^^

Initializes the Matter color temperature light endpoint with optional initial state, brightness, and color temperature.

.. code-block:: arduino

    bool begin(bool initialState = false, uint8_t brightness = 64, uint16_t colorTemperature = 370);

* ``initialState`` - Initial on/off state (default: ``false`` = off)
* ``brightness`` - Initial brightness level (0-255, default: 64 = 25%)
* ``colorTemperature`` - Initial color temperature in mireds (100-500, default: 370 = Soft White)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter light events.

.. code-block:: arduino

    void end();

Constants
*********

MAX_BRIGHTNESS
^^^^^^^^^^^^^^

Maximum brightness value (255).

.. code-block:: arduino

    static const uint8_t MAX_BRIGHTNESS = 255;

MAX_COLOR_TEMPERATURE
^^^^^^^^^^^^^^^^^^^^^

Maximum color temperature value (500 mireds = cool white).

.. code-block:: arduino

    static const uint16_t MAX_COLOR_TEMPERATURE = 500;

MIN_COLOR_TEMPERATURE
^^^^^^^^^^^^^^^^^^^^^

Minimum color temperature value (100 mireds = warm white).

.. code-block:: arduino

    static const uint16_t MIN_COLOR_TEMPERATURE = 100;

On/Off Control
**************

setOnOff
^^^^^^^^

Sets the on/off state of the light.

.. code-block:: arduino

    bool setOnOff(bool newState);

getOnOff
^^^^^^^^

Gets the current on/off state.

.. code-block:: arduino

    bool getOnOff();

toggle
^^^^^^

Toggles the on/off state.

.. code-block:: arduino

    bool toggle();

Brightness Control
******************

setBrightness
^^^^^^^^^^^^^

Sets the brightness level.

.. code-block:: arduino

    bool setBrightness(uint8_t newBrightness);

* ``newBrightness`` - Brightness level (0-255)

getBrightness
^^^^^^^^^^^^^

Gets the current brightness level.

.. code-block:: arduino

    uint8_t getBrightness();

Color Temperature Control
*************************

setColorTemperature
^^^^^^^^^^^^^^^^^^^^

Sets the color temperature.

.. code-block:: arduino

    bool setColorTemperature(uint16_t newTemperature);

* ``newTemperature`` - Color temperature in mireds (100-500)

**Note:** Color temperature is measured in mireds (micro reciprocal degrees). Lower values (100-200) are warm white, higher values (400-500) are cool white.

getColorTemperature
^^^^^^^^^^^^^^^^^^^^

Gets the current color temperature.

.. code-block:: arduino

    uint16_t getColorTemperature();

Event Handling
**************

onChange
^^^^^^^^

Sets a callback for when any parameter changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState, uint8_t newBrightness, uint16_t newTemperature);

onChangeOnOff
^^^^^^^^^^^^^

Sets a callback for on/off state changes.

.. code-block:: arduino

    void onChangeOnOff(EndPointOnOffCB onChangeCB);

onChangeBrightness
^^^^^^^^^^^^^^^^^^

Sets a callback for brightness changes.

.. code-block:: arduino

    void onChangeBrightness(EndPointBrightnessCB onChangeCB);

onChangeColorTemperature
^^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback for color temperature changes.

.. code-block:: arduino

    void onChangeColorTemperature(EndPointTemperatureCB onChangeCB);

updateAccessory
^^^^^^^^^^^^^^^

Updates the physical light state using current Matter internal state.

.. code-block:: arduino

    void updateAccessory();

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns current on/off state.

.. code-block:: arduino

    operator bool();

Assignment operator
^^^^^^^^^^^^^^^^^^^

Turns light on or off.

.. code-block:: arduino

    void operator=(bool state);

Example
-------

Color Temperature Light
************************

.. literalinclude:: ../../../libraries/Matter/examples/MatterColorTemperatureLight/MatterColorTemperatureLight.ino
    :language: arduino

