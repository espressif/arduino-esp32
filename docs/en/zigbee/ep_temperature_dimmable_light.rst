##############################
ZigbeeTemperatureDimmableLight
##############################

About
-----

The ``ZigbeeTemperatureDimmableLight`` class provides an endpoint for tunable white (temperature dimmable) lights in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for color temperature lighting devices, supporting on/off, dimming, color temperature control, and scene management.

**Features:**
* On/off control
* Brightness level control (0-100%)
* Color temperature control (mireds)
* Scene and group support
* Automatic state restoration
* Min/max color temperature configuration
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Smart tunable white bulbs
* Adjustable white LED panels
* Human-centric lighting
* Office and home white lighting
* Smart home white ambiance lighting

API Reference
-------------

Constructor
***********

ZigbeeTemperatureDimmableLight
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee temperature dimmable light endpoint.

.. code-block:: arduino

    ZigbeeTemperatureDimmableLight(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Callback Functions
******************

onLightChange
^^^^^^^^^^^^^

Sets the callback function for light state changes.

.. code-block:: arduino

    void onLightChange(void (*callback)(bool, uint8_t, uint16_t));

* ``callback`` - Function pointer to the light change callback (state, level, mireds)

Control Methods
***************

setLightState
^^^^^^^^^^^^^

Sets the light on/off state.

.. code-block:: arduino

    bool setLightState(bool state);

* ``state`` - Light state (true = on, false = off)

Returns ``true`` if successful, ``false`` otherwise.

setLightLevel
^^^^^^^^^^^^^

Sets the light brightness level.

.. code-block:: arduino

    bool setLightLevel(uint8_t level);

* ``level`` - Brightness level (0-255, where 0 is off, 255 is full brightness)

Returns ``true`` if successful, ``false`` otherwise.

setLightTemperature
^^^^^^^^^^^^^^^^^^^

Sets the light color temperature (in mireds).

.. code-block:: arduino

    bool setLightTemperature(uint16_t mireds);

* ``mireds`` - Color temperature in mireds (153 = 6500K, 500 = 2000K)

Returns ``true`` if successful, ``false`` otherwise.

setLight
^^^^^^^^

Sets all light parameters at once.

.. code-block:: arduino

    bool setLight(bool state, uint8_t level, uint16_t mireds);

* ``state`` - Light state (true/false)
* ``level`` - Brightness level (0-255)
* ``mireds`` - Color temperature in mireds

Returns ``true`` if successful, ``false`` otherwise.

setMinMaxTemperature
^^^^^^^^^^^^^^^^^^^^

Sets the minimum and maximum allowed color temperature (in mireds).

.. code-block:: arduino

    bool setMinMaxTemperature(uint16_t min_temp, uint16_t max_temp);

* ``min_temp`` - Minimum color temperature in mireds (e.g., 153 for 6500K)
* ``max_temp`` - Maximum color temperature in mireds (e.g., 500 for 2000K)

Returns ``true`` if successful, ``false`` otherwise.

State Retrieval Methods
***********************

getLightState
^^^^^^^^^^^^^

Gets the current light state.

.. code-block:: arduino

    bool getLightState();

Returns current light state (true = on, false = off).

getLightLevel
^^^^^^^^^^^^^

Gets the current brightness level.

.. code-block:: arduino

    uint8_t getLightLevel();

Returns current brightness level (0-255).

getLightTemperature
^^^^^^^^^^^^^^^^^^^

Gets the current color temperature (in mireds).

.. code-block:: arduino

    uint16_t getLightTemperature();

Returns current color temperature in mireds.

Utility Methods
***************

restoreLight
^^^^^^^^^^^^

Restores the light to its last known state.

.. code-block:: arduino

    void restoreLight();

Example
-------

Temperature Dimmable Light Implementation
*****************************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Temperature_Dimmable_Light/Zigbee_Temperature_Dimmable_Light.ino
    :language: arduino
