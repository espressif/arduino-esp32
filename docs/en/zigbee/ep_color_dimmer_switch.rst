#######################
ZigbeeColorDimmerSwitch
#######################

About
-----

The ``ZigbeeColorDimmerSwitch`` class provides a color dimmer switch endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for advanced lighting control switches that can control both dimming and color of lights.

**Features:**
* On/off control for bound lights
* Brightness level control (0-100%)
* Color control (RGB, HSV)
* Color temperature control
* Scene and group support
* Special effects and timed operations
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Smart lighting switches
* Color control remotes
* Advanced lighting controllers
* Smart home lighting automation
* Entertainment lighting control

API Reference
-------------

Constructor
***********

ZigbeeColorDimmerSwitch
^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee color dimmer switch endpoint.

.. code-block:: arduino

    ZigbeeColorDimmerSwitch(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Basic Control Commands
**********************

lightToggle
^^^^^^^^^^^

Toggles the state of bound lights (on to off, or off to on).

.. code-block:: arduino

    void lightToggle();
    void lightToggle(uint16_t group_addr);
    void lightToggle(uint8_t endpoint, uint16_t short_addr);
    void lightToggle(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``group_addr`` - Group address to control (optional)
* ``endpoint`` - Target device endpoint (optional)
* ``short_addr`` - Target device short address (optional)
* ``ieee_addr`` - Target device IEEE address (optional)

lightOn
^^^^^^^

Turns on bound lights.

.. code-block:: arduino

    void lightOn();
    void lightOn(uint16_t group_addr);
    void lightOn(uint8_t endpoint, uint16_t short_addr);
    void lightOn(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``group_addr`` - Group address to control (optional)
* ``endpoint`` - Target device endpoint (optional)
* ``short_addr`` - Target device short address (optional)
* ``ieee_addr`` - Target device IEEE address (optional)

lightOff
^^^^^^^^

Turns off bound lights.

.. code-block:: arduino

    void lightOff();
    void lightOff(uint16_t group_addr);
    void lightOff(uint8_t endpoint, uint16_t short_addr);
    void lightOff(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``group_addr`` - Group address to control (optional)
* ``endpoint`` - Target device endpoint (optional)
* ``short_addr`` - Target device short address (optional)
* ``ieee_addr`` - Target device IEEE address (optional)

Dimmer Control Commands
***********************

setLightLevel
^^^^^^^^^^^^^

Sets the brightness level of bound lights.

.. code-block:: arduino

    void setLightLevel(uint8_t level);
    void setLightLevel(uint8_t level, uint16_t group_addr);
    void setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr);
    void setLightLevel(uint8_t level, uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``level`` - Brightness level (0-100, where 0 is off, 100 is full brightness)
* ``group_addr`` - Group address to control (optional)
* ``endpoint`` - Target device endpoint (optional)
* ``short_addr`` - Target device short address (optional)
* ``ieee_addr`` - Target device IEEE address (optional)

setLightLevelStep
^^^^^^^^^^^^^^^^^

Sends a level step command to bound lights: changes the current level by a fixed step size in the given direction.

.. code-block:: arduino

    void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time);
    void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint16_t group_addr);
    void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint8_t endpoint, uint16_t short_addr);
    void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``direction`` - ``ZIGBEE_LEVEL_STEP_UP`` (0) or ``ZIGBEE_LEVEL_STEP_DOWN`` (1)
* ``step_size`` - Number of level units to step (e.g. 1–254; one step = one unit change in CurrentLevel)
* ``transition_time`` - Time to perform the step, in tenths of a second (see below)

**Transition time (ZCL Step command):**

* The transition time is the duration, in **tenths of a second**, that the device should take to perform the step. A step is a change in CurrentLevel of ``step_size`` units.
* The device should take as close to this time as it is able. If the device cannot move at a variable rate, it may disregard the transition time and move at a fixed rate.
* Use **0xFFFF** (65535) to request the device to move **as fast as it is able** (no specific duration).

**Examples:**

* Step level up by 10 units over 1 second: ``setLightLevelStep(ZIGBEE_LEVEL_STEP_UP, 10, 10);``
* Step level down by 5 units as fast as possible: ``setLightLevelStep(ZIGBEE_LEVEL_STEP_DOWN, 5, 0xFFFF);``

Color Control Commands
**********************

setLightColor
^^^^^^^^^^^^^

Sets the color of bound lights using RGB values.

.. code-block:: arduino

    void setLightColor(uint8_t red, uint8_t green, uint8_t blue);
    void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr);
    void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr);
    void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

* ``red`` - Red component (0-255)
* ``green`` - Green component (0-255)
* ``blue`` - Blue component (0-255)
* ``group_addr`` - Group address to control (optional)
* ``endpoint`` - Target device endpoint (optional)
* ``short_addr`` - Target device short address (optional)
* ``ieee_addr`` - Target device IEEE address (optional)

Advanced Control Commands
*************************

lightOffWithEffect
^^^^^^^^^^^^^^^^^^

Turns off lights with a specific effect.

.. code-block:: arduino

    void lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant);

* ``effect_id`` - Effect identifier
* ``effect_variant`` - Effect variant

lightOnWithTimedOff
^^^^^^^^^^^^^^^^^^^

Turns on lights with automatic turn-off after specified time.

.. code-block:: arduino

    void lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off);

* ``on_off_control`` - Control byte
* ``time_on`` - Time to stay on (in 1/10th seconds)
* ``time_off`` - Time to stay off (in 1/10th seconds)

lightOnWithSceneRecall
^^^^^^^^^^^^^^^^^^^^^^

Turns on lights with scene recall.

.. code-block:: arduino

    void lightOnWithSceneRecall();

Example
-------

Color Dimmer Switch Implementation
**********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Color_Dimmer_Switch/Zigbee_Color_Dimmer_Switch.ino
    :language: arduino
