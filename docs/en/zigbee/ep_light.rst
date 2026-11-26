###########
ZigbeeLight
###########

About
-----

The ``ZigbeeLight`` class provides a simple on/off light endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for basic lighting control.

**Features:**
* Simple on/off control
* ``OnTime``, ``OffWaitTime``, and ``GlobalSceneControl``, so Off with effect turns the light off
* State change callbacks

API Reference
-------------

Constructor
***********

ZigbeeLight
^^^^^^^^^^^

Creates a new Zigbee light endpoint.

.. code-block:: arduino

    ZigbeeLight(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Light Control
*************

setLight
^^^^^^^^

Sets the light state (on or off).

.. code-block:: arduino

    bool setLight(bool state);

* ``state`` - ``true`` to turn on, ``false`` to turn off

This function will return ``true`` if successful, ``false`` otherwise.

getLightState
^^^^^^^^^^^^^

Gets the current light state.

.. code-block:: arduino

    bool getLightState();

This function will return current light state (``true`` = on, ``false`` = off).

restoreLight
^^^^^^^^^^^^

Restores the light state and triggers any registered callbacks.

.. code-block:: arduino

    void restoreLight();

Event Handling
**************

onLightChange
^^^^^^^^^^^^^

Sets a callback function to be called when the light state changes.

.. code-block:: arduino

    void onLightChange(void (*callback)(bool));

* ``callback`` - Function to call when light state changes

Example
-------

Basic Light Implementation
**************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_On_Off_Light/Zigbee_On_Off_Light.ino
    :language: arduino

Philips Hue Bridge
******************

``Zigbee_Hue_Light`` is a router on/off light that can join a Philips Hue Bridge. After ``role()``
the sketch enables distributed security, passes a 16-byte Trust Center link key, and sets the
endpoint version to 1. Replace the placeholder key in the sketch with a Hue Trust Center link key.
The library does not ship that key. Public copies that work with Hue are available online.

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Hue_Light/Zigbee_Hue_Light.ino
    :language: arduino
