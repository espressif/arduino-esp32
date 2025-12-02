########################
ZigbeeColorDimmableLight
########################

About
-----

The ``ZigbeeColorDimmableLight`` class provides an endpoint for color dimmable lights in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for color lighting devices, supporting multiple color modes (RGB/XY, HSV, and Color Temperature), dimming, and scene management.

**Features:**
* On/off control
* Brightness level control (0-255)
* RGB/XY color control
* HSV (Hue/Saturation) color support
* Color temperature (mireds) support
* Configurable color capabilities (enable/disable color modes)
* Separate callbacks for RGB, HSV, and Temperature modes
* Automatic color mode switching
* Scene and group support
* Automatic state restoration
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Smart RGB light bulbs
* Color-changing LED strips
* Tunable white light bulbs
* Full-spectrum color temperature lights
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

Color Capabilities
******************

setLightColorCapabilities
^^^^^^^^^^^^^^^^^^^^^^^^^

Configures which color modes are supported by the light. Must be called before starting Zigbee. By default, only XY (RGB) mode is enabled.

.. code-block:: arduino

    bool setLightColorCapabilities(uint16_t capabilities);

* ``capabilities`` - Bit flags indicating supported color modes (can be combined with bitwise OR):
  
  * ``ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION`` - Hue/Saturation support
  * ``ZIGBEE_COLOR_CAPABILITY_X_Y`` - XY (RGB) support
  * ``ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP`` - Color temperature support
  * ``ZIGBEE_COLOR_CAPABILITY_ENHANCED_HUE`` - Enhanced hue support
  * ``ZIGBEE_COLOR_CAPABILITY_COLOR_LOOP`` - Color loop support

**Example:**

.. code-block:: arduino

    // Enable XY and Temperature modes
    light.setLightColorCapabilities(
        ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP
    );
    
    // Enable all color modes
    light.setLightColorCapabilities(
        ZIGBEE_COLOR_CAPABILITY_X_Y | 
        ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION | 
        ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP
    );

This function will return ``true`` if successful, ``false`` otherwise.

Callback Functions
******************

onLightChange
^^^^^^^^^^^^^

.. deprecated:: This method is deprecated and will be removed in a future major version. Use ``onLightChangeRgb()`` instead.

Sets the legacy callback function for light state changes (RGB mode).

.. code-block:: arduino

    void onLightChange(void (*callback)(bool, uint8_t, uint8_t, uint8_t, uint8_t));

* ``callback`` - Function pointer to the light change callback (state, red, green, blue, level)

.. note::
   This method is deprecated. Please use ``onLightChangeRgb()`` for RGB/XY mode callbacks.

onLightChangeRgb
^^^^^^^^^^^^^^^^

Sets the callback function for RGB/XY color mode changes.

.. code-block:: arduino

    void onLightChangeRgb(void (*callback)(bool, uint8_t, uint8_t, uint8_t, uint8_t));

* ``callback`` - Function pointer to the RGB light change callback (state, red, green, blue, level)

onLightChangeHsv
^^^^^^^^^^^^^^^^

Sets the callback function for HSV (Hue/Saturation) color mode changes.

.. code-block:: arduino

    void onLightChangeHsv(void (*callback)(bool, uint8_t, uint8_t, uint8_t, uint8_t));

* ``callback`` - Function pointer to the HSV light change callback (state, hue, saturation, value, level)

onLightChangeTemp
^^^^^^^^^^^^^^^^^

Sets the callback function for color temperature mode changes.

.. code-block:: arduino

    void onLightChangeTemp(void (*callback)(bool, uint8_t, uint16_t));

* ``callback`` - Function pointer to the temperature light change callback (state, level, temperature_mireds)

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

* ``level`` - Brightness level (0-255, where 0 is off, 255 is full brightness)

This function will return ``true`` if successful, ``false`` otherwise.

setLightColor (RGB)
^^^^^^^^^^^^^^^^^^^

Sets the light color using RGB values. Requires ``ZIGBEE_COLOR_CAPABILITY_X_Y`` capability to be enabled.

.. code-block:: arduino

    bool setLightColor(uint8_t red, uint8_t green, uint8_t blue);
    bool setLightColor(espRgbColor_t rgb_color);

* ``red`` - Red component (0-255)
* ``green`` - Green component (0-255)
* ``blue`` - Blue component (0-255)
* ``rgb_color`` - RGB color structure

This function will return ``true`` if successful, ``false`` otherwise. Returns ``false`` if XY capability is not enabled.

setLightColor (HSV)
^^^^^^^^^^^^^^^^^^^

Sets the light color using HSV values. Requires ``ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION`` capability to be enabled.

.. code-block:: arduino

    bool setLightColor(espHsvColor_t hsv_color);

* ``hsv_color`` - HSV color structure

This function will return ``true`` if successful, ``false`` otherwise. Returns ``false`` if HSV capability is not enabled.

setLightColorTemperature
^^^^^^^^^^^^^^^^^^^^^^^^

Sets the light color temperature in mireds. Requires ``ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP`` capability to be enabled.

.. code-block:: arduino

    bool setLightColorTemperature(uint16_t color_temperature);

* ``color_temperature`` - Color temperature in mireds (inverse of Kelvin: mireds = 1000000 / Kelvin)

**Example:**

.. code-block:: arduino

    // Set to 4000K (cool white)
    uint16_t mireds = 1000000 / 4000;  // = 250 mireds
    light.setLightColorTemperature(mireds);
    
    // Set to 2700K (warm white)
    mireds = 1000000 / 2700;  // = 370 mireds
    light.setLightColorTemperature(mireds);

This function will return ``true`` if successful, ``false`` otherwise. Returns ``false`` if color temperature capability is not enabled.

setLightColorTemperatureRange
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the minimum and maximum color temperature range supported by the hardware.

.. code-block:: arduino

    bool setLightColorTemperatureRange(uint16_t min_temp, uint16_t max_temp);

* ``min_temp`` - Minimum color temperature in mireds
* ``max_temp`` - Maximum color temperature in mireds

**Example:**

.. code-block:: arduino

    // Set range for 2000K (warm) to 6500K (cool)
    uint16_t min_mireds = 1000000 / 6500;  // = 154 mireds
    uint16_t max_mireds = 1000000 / 2000;  // = 500 mireds
    light.setLightColorTemperatureRange(min_mireds, max_mireds);

This function will return ``true`` if successful, ``false`` otherwise.

setLight
^^^^^^^^

Sets all light parameters at once (RGB mode). Requires ``ZIGBEE_COLOR_CAPABILITY_X_Y`` capability to be enabled.

.. code-block:: arduino

    bool setLight(bool state, uint8_t level, uint8_t red, uint8_t green, uint8_t blue);

* ``state`` - Light state (true/false)
* ``level`` - Brightness level (0-255)
* ``red`` - Red component (0-255)
* ``green`` - Green component (0-255)
* ``blue`` - Blue component (0-255)

This function will return ``true`` if successful, ``false`` otherwise. Returns ``false`` if XY capability is not enabled.

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

This function will return current brightness level (0-255).

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

getLightColorTemperature
^^^^^^^^^^^^^^^^^^^^^^^^

Gets the current color temperature.

.. code-block:: arduino

    uint16_t getLightColorTemperature();

This function will return current color temperature in mireds.

getLightColorMode
^^^^^^^^^^^^^^^^^

Gets the current active color mode.

.. code-block:: arduino

    uint8_t getLightColorMode();

This function will return current color mode:
* ``ZIGBEE_COLOR_MODE_HUE_SATURATION`` (0x00) - HSV mode
* ``ZIGBEE_COLOR_MODE_CURRENT_X_Y`` (0x01) - XY/RGB mode
* ``ZIGBEE_COLOR_MODE_TEMPERATURE`` (0x02) - Temperature mode

getLightColorHue
^^^^^^^^^^^^^^^^

Gets the current hue value (HSV mode).

.. code-block:: arduino

    uint8_t getLightColorHue();

This function will return current hue value (0-254).

getLightColorSaturation
^^^^^^^^^^^^^^^^^^^^^^^

Gets the current saturation value (HSV mode).

.. code-block:: arduino

    uint8_t getLightColorSaturation();

This function will return current saturation value (0-254).

getLightColorCapabilities
^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the currently configured color capabilities.

.. code-block:: arduino

    uint16_t getLightColorCapabilities();

This function will return the current color capabilities bit flags.

Utility Methods
***************

restoreLight
^^^^^^^^^^^^

Restores the light to its last known state. Uses the appropriate callback based on the current color mode.

.. code-block:: arduino

    void restoreLight();

Color Modes and Automatic Behavior
**********************************

The ``ZigbeeColorDimmableLight`` class supports three color modes:

* **XY/RGB Mode** (``ZIGBEE_COLOR_MODE_CURRENT_X_Y``) - Uses X/Y coordinates for color representation, internally converted to RGB
* **HSV Mode** (``ZIGBEE_COLOR_MODE_HUE_SATURATION``) - Uses Hue and Saturation values directly, without RGB conversion
* **Temperature Mode** (``ZIGBEE_COLOR_MODE_TEMPERATURE``) - Uses color temperature in mireds

**Automatic Mode Switching:**

The color mode is automatically updated based on which attributes are set:

* Setting RGB colors via ``setLight()`` or ``setLightColor()`` (RGB) → switches to XY mode
* Setting HSV colors via ``setLightColor()`` (HSV) → switches to HSV mode
* Setting temperature via ``setLightColorTemperature()`` → switches to Temperature mode
* Receiving Zigbee commands for XY/HSV/TEMP attributes → automatically switches to the corresponding mode

**Capability Validation:**

All set methods validate that the required capability is enabled before allowing the operation:

* RGB/XY methods require ``ZIGBEE_COLOR_CAPABILITY_X_Y``
* HSV methods require ``ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION``
* Temperature methods require ``ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP``

If a capability is not enabled, the method will return ``false`` and log an error.

**Callback Selection:**

The appropriate callback is automatically called based on the current color mode:

* RGB/XY mode → ``onLightChangeRgb()`` callback
* HSV mode → ``onLightChangeHsv()`` callback
* Temperature mode → ``onLightChangeTemp()`` callback

When level or state changes occur, the callback for the current color mode is used automatically.

.. note::
   The legacy ``onLightChange()`` callback is deprecated and will be removed in a future major version. Always use the mode-specific callbacks (``onLightChangeRgb()``, ``onLightChangeHsv()``, or ``onLightChangeTemp()``).

Example
-------

Color Dimmable Light Implementation
***********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Color_Dimmable_Light/Zigbee_Color_Dimmable_Light.ino
    :language: arduino
