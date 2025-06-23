#################
ZigbeePowerOutlet
#################

About
-----

The ``ZigbeePowerOutlet`` class provides a smart power outlet endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for power control, allowing remote on/off control of electrical devices.

**Features:**
* On/off power control
* State change callbacks

API Reference
-------------

Constructor
***********

ZigbeePowerOutlet
^^^^^^^^^^^^^^^^^

Creates a new Zigbee power outlet endpoint.

.. code-block:: arduino

    ZigbeePowerOutlet(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Power Control
*************

setState
^^^^^^^^

Sets the power outlet state (on or off).

.. code-block:: arduino

    bool setState(bool state);

* ``state`` - ``true`` to turn on, ``false`` to turn off

This function will return ``true`` if successful, ``false`` otherwise.

getPowerOutletState
^^^^^^^^^^^^^^^^^^^

Gets the current power outlet state.

.. code-block:: arduino

    bool getPowerOutletState();

This function will return current power state (``true`` = on, ``false`` = off).

restoreState
^^^^^^^^^^^^

Restores the power outlet state and triggers any registered callbacks.

.. code-block:: arduino

    void restoreState();

Event Handling
**************

onPowerOutletChange
^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when the power outlet state changes.

.. code-block:: arduino

    void onPowerOutletChange(void (*callback)(bool));

* ``callback`` - Function to call when power outlet state changes

Example
-------

Smart Power Outlet Implementation
*********************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Power_Outlet/Zigbee_Power_Outlet.ino
    :language: arduino
