###################
ZigbeeDimmableLight
###################

About
-----

The ``ZigbeeDimmableLight`` class provides a dimmable light endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for dimmable lighting control with both on/off and brightness level control.

**Features:**
* On/off control
* Brightness level control (0-100%)
* State and level change callbacks
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Dimmable smart bulbs
* LED strips with brightness control
* Any device requiring both on/off and dimming functionality

API Reference
-------------

Constructor
***********

ZigbeeDimmableLight
^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee dimmable light endpoint.

.. code-block:: arduino

    ZigbeeDimmableLight(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Light Control
*************

setLightState
^^^^^^^^^^^^^

Sets only the light state (on or off) without changing the brightness level.

.. code-block:: arduino

    bool setLightState(bool state);

* ``state`` - ``true`` to turn on, ``false`` to turn off

This function will return ``true`` if successful, ``false`` otherwise.

setLightLevel
^^^^^^^^^^^^^

Sets only the brightness level (0-100) without changing the on/off state.

.. code-block:: arduino

    bool setLightLevel(uint8_t level);

* ``level`` - Brightness level (0-100, where 0 is off, 100 is full brightness)

This function will return ``true`` if successful, ``false`` otherwise.

setLight
^^^^^^^^

Sets both the light state and brightness level simultaneously.

.. code-block:: arduino

    bool setLight(bool state, uint8_t level);

* ``state`` - ``true`` to turn on, ``false`` to turn off
* ``level`` - Brightness level (0-100)

This function will return ``true`` if successful, ``false`` otherwise.

getLightState
^^^^^^^^^^^^^

Gets the current light state.

.. code-block:: arduino

    bool getLightState();

This function will return current light state (``true`` = on, ``false`` = off).

getLightLevel
^^^^^^^^^^^^^

Gets the current brightness level.

.. code-block:: arduino

    uint8_t getLightLevel();

This function will return current brightness level (0-100).

restoreLight
^^^^^^^^^^^^

Restores the light state and triggers any registered callbacks.

.. code-block:: arduino

    void restoreLight();

Event Handling
**************

onLightChange
^^^^^^^^^^^^^

Sets a callback function to be called when the light state or level changes.

.. code-block:: arduino

    void onLightChange(void (*callback)(bool, uint8_t));

* ``callback`` - Function to call when light state or level changes

**Callback Parameters:**
* ``bool state`` - New light state (true = on, false = off)
* ``uint8_t level`` - New brightness level (0-100)

Example
-------

Dimmable Light Implementation
*****************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Dimmable_Light/Zigbee_Dimmable_Light.ino
    :language: arduino
