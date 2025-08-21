############
ZigbeeSwitch
############

About
-----

The ``ZigbeeSwitch`` class provides a switch endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for controlling other devices (typically lights) through on/off commands.

**Features:**
* On/off control commands for bound devices
* Group control support
* Direct device addressing

API Reference
-------------

Constructor
***********

ZigbeeSwitch
^^^^^^^^^^^^

Creates a new Zigbee switch endpoint.

.. code-block:: arduino

    ZigbeeSwitch(uint8_t endpoint);

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

Turns on lights by recalling a scene.

.. code-block:: arduino

    void lightOnWithSceneRecall();

Example
-------

Basic Switch Implementation
***************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_On_Off_Switch/Zigbee_On_Off_Switch.ino
    :language: arduino

Multi Switch Implementation
***************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_On_Off_MultiSwitch/Zigbee_On_Off_MultiSwitch.ino
    :language: arduino
