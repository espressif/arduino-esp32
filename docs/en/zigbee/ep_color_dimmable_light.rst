########################
ZigbeeColorDimmableLight
########################

About
-----

The ``ZigbeeColorDimmableLight`` class provides an endpoint for color dimmable lights in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for color lighting devices, supporting RGB color control, dimming, and scene management.

**Features:**
* On/off control
* Brightness level control (0-100%)
* RGB color control
* HSV color support
* Scene and group support
* Automatic state restoration
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Smart RGB light bulbs
* Color-changing LED strips
* Mood lighting systems
* Entertainment lighting
* Architectural lighting
* Smart home color lighting

API Reference
-------------

Constructor
***********

ZigbeeColorDimmableLight
^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee color dimmable light endpoint.

.. code-block:: arduino

    ZigbeeColorDimmableLight(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Callback Functions
******************

onLightChange
^^^^^^^^^^^^^

Sets the callback function for light state changes.

.. code-block:: arduino

    void onLightChange(void (*callback)(bool, uint8_t, uint8_t, uint8_t, uint8_t));

* ``callback`` - Function pointer to the light change callback (state, red, green, blue, level)

Control Methods
***************

setLightState
^^^^^^^^^^^^^

Sets the light on/off state.

.. code-block:: arduino

    bool setLightState(bool state);

* ``state`` - Light state (true = on, false = off)

This function will return ``true`` if successful, ``false`` otherwise.

setLightLevel
^^^^^^^^^^^^^

Sets the light brightness level.

.. code-block:: arduino

    bool setLightLevel(uint8_t level);

* ``level`` - Brightness level (0-100, where 0 is off, 100 is full brightness)

This function will return ``true`` if successful, ``false`` otherwise.

setLightColor (RGB)
^^^^^^^^^^^^^^^^^^^

Sets the light color using RGB values.

.. code-block:: arduino

    bool setLightColor(uint8_t red, uint8_t green, uint8_t blue);
    bool setLightColor(espRgbColor_t rgb_color);

* ``red`` - Red component (0-255)
* ``green`` - Green component (0-255)
* ``blue`` - Blue component (0-255)
* ``rgb_color`` - RGB color structure

This function will return ``true`` if successful, ``false`` otherwise.

setLightColor (HSV)
^^^^^^^^^^^^^^^^^^^

Sets the light color using HSV values.

.. code-block:: arduino

    bool setLightColor(espHsvColor_t hsv_color);

* ``hsv_color`` - HSV color structure

This function will return ``true`` if successful, ``false`` otherwise.

setLight
^^^^^^^^

Sets all light parameters at once.

.. code-block:: arduino

    bool setLight(bool state, uint8_t level, uint8_t red, uint8_t green, uint8_t blue);

* ``state`` - Light state (true/false)
* ``level`` - Brightness level (0-100)
* ``red`` - Red component (0-255)
* ``green`` - Green component (0-255)
* ``blue`` - Blue component (0-255)

This function will return ``true`` if successful, ``false`` otherwise.

State Retrieval Methods
***********************

getLightState
^^^^^^^^^^^^^

Gets the current light state.

.. code-block:: arduino

    bool getLightState();

This function will return current light state (true = on, false = off).

getLightLevel
^^^^^^^^^^^^^

Gets the current brightness level.

.. code-block:: arduino

    uint8_t getLightLevel();

This function will return current brightness level (0-100).

getLightColor
^^^^^^^^^^^^^

Gets the current RGB color.

.. code-block:: arduino

    espRgbColor_t getLightColor();

This function will return current RGB color structure.

getLightRed
^^^^^^^^^^^

Gets the current red component.

.. code-block:: arduino

    uint8_t getLightRed();

This function will return current red component (0-255).

getLightGreen
^^^^^^^^^^^^^

Gets the current green component.

.. code-block:: arduino

    uint8_t getLightGreen();

This function will return current green component (0-255).

getLightBlue
^^^^^^^^^^^^

Gets the current blue component.

.. code-block:: arduino

    uint8_t getLightBlue();

This function will return current blue component (0-255).

Utility Methods
***************

restoreLight
^^^^^^^^^^^^

Restores the light to its last known state.

.. code-block:: arduino

    void restoreLight();

Example
-------

Color Dimmable Light Implementation
***********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Color_Dimmable_Light/Zigbee_Color_Dimmable_Light.ino
    :language: arduino
