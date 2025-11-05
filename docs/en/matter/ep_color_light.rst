###############
MatterColorLight
###############

About
-----

The ``MatterColorLight`` class provides a color light endpoint for Matter networks with RGB color control using the HSV color model. This endpoint implements the Matter lighting standard for full-color lighting control.

**Features:**
* On/off control
* RGB color control with HSV color model
* State persistence support
* Callback support for state and color changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* RGB smart lights
* Color-changing lights
* Mood lighting
* Entertainment lighting control
* Smart home color automation

API Reference
-------------

Constructor
***********

MatterColorLight
^^^^^^^^^^^^^^^^

Creates a new Matter color light endpoint.

.. code-block:: arduino

    MatterColorLight();

Initialization
**************

begin
^^^^^

Initializes the Matter color light endpoint with optional initial state and color.

.. code-block:: arduino

    bool begin(bool initialState = false, espHsvColor_t colorHSV = {0, 254, 31});

* ``initialState`` - Initial on/off state (default: ``false`` = off)
* ``colorHSV`` - Initial HSV color (default: red 12% intensity HSV(0, 254, 31))

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

* ``rgbColor`` - RGB color structure with red, green, and blue values (0-255 each)

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

* ``hsvColor`` - HSV color structure with hue (0-360), saturation (0-254), and value/brightness (0-254)

getColorHSV
^^^^^^^^^^^

Gets the current color as HSV values.

.. code-block:: arduino

    espHsvColor_t getColorHSV();

Event Handling
**************

onChange
^^^^^^^^

Sets a callback for when any parameter changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(bool newState, espHsvColor_t newColor);

onChangeOnOff
^^^^^^^^^^^^^

Sets a callback for on/off state changes.

.. code-block:: arduino

    void onChangeOnOff(EndPointOnOffCB onChangeCB);

onChangeColorHSV
^^^^^^^^^^^^^^^^

Sets a callback for color changes.

.. code-block:: arduino

    void onChangeColorHSV(EndPointRGBColorCB onChangeCB);

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

Color Light
***********

.. literalinclude:: ../../../libraries/Matter/examples/MatterColorLight/MatterColorLight.ino
    :language: arduino

