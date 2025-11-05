########################
MatterEnhancedColorLight
########################

About
-----

The ``MatterEnhancedColorLight`` class provides an enhanced color light endpoint for Matter networks with full RGB color control, brightness, and color temperature. This endpoint implements the Matter lighting standard for advanced color lighting with all features.

**Features:**
* On/off control
* RGB color control with HSV color model
* Brightness level control (0-255)
* Color temperature control (100-500 mireds)
* State persistence support
* Callback support for all parameter changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Full-featured RGB smart lights
* Advanced color and temperature control
* Mood lighting with all features
* Entertainment lighting
* Circadian lighting with color temperature
* Smart home advanced lighting automation

API Reference
-------------

Constructor
***********

MatterEnhancedColorLight
^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter enhanced color light endpoint.

.. code-block:: arduino

    MatterEnhancedColorLight();

Initialization
**************

begin
^^^^^

Initializes the Matter enhanced color light endpoint with optional initial state, color, brightness, and color temperature.

.. code-block:: arduino

    bool begin(bool initialState = false, espHsvColor_t colorHSV = {21, 216, 25}, uint8_t newBrightness = 25, uint16_t colorTemperature = 454);

* ``initialState`` - Initial on/off state (default: ``false`` = off)
* ``colorHSV`` - Initial HSV color (default: HSV(21, 216, 25) = warm white)
* ``newBrightness`` - Initial brightness level (0-255, default: 25 = 10%)
* ``colorTemperature`` - Initial color temperature in mireds (100-500, default: 454 = Warm White)

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

Color Control
************

setColorRGB
^^^^^^^^^^^

Sets the color using RGB values.

.. code-block:: arduino

    bool setColorRGB(espRgbColor_t rgbColor);

getColorRGB
^^^^^^^^^^^

Gets the current color as RGB values.

.. code-block:: arduino

    espRgbColor_t getColorRGB();

setColorHSV
^^^^^^^^^^^

Sets the color using HSV values.

.. code-block:: arduino

    bool setColorHSV(espHsvColor_t hsvColor);

getColorHSV
^^^^^^^^^^^

Gets the current color as HSV values.

.. code-block:: arduino

    espHsvColor_t getColorHSV();

Brightness Control
******************

setBrightness
^^^^^^^^^^^^^

Sets the brightness level.

.. code-block:: arduino

    bool setBrightness(uint8_t newBrightness);

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

    bool onChangeCallback(bool newState, espHsvColor_t newColor, uint8_t newBrightness, uint16_t newTemperature);

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

onChangeColorHSV
^^^^^^^^^^^^^^^^

Sets a callback for color changes.

.. code-block:: arduino

    void onChangeColorHSV(EndPointRGBColorCB onChangeCB);

onChangeColorTemperature
^^^^^^^^^^^^^^^^^^^^^^^^^

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

Enhanced Color Light
********************

.. literalinclude:: ../../../libraries/Matter/examples/MatterEnhancedColorLight/MatterEnhancedColorLight.ino
    :language: arduino

