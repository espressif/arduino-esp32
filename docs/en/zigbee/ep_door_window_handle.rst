######################
ZigbeeDoorWindowHandle
######################

About
-----

The ``ZigbeeDoorWindowHandle`` class provides a door/window handle endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for handle position sensors and other handle-related devices.

**Features:**
* Handle position detection
* Multiple position states support
* Configurable application types
* Automatic reporting capabilities


API Reference
-------------

Constructor
***********

ZigbeeDoorWindowHandle
^^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee door/window handle endpoint.

.. code-block:: arduino

    ZigbeeDoorWindowHandle(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setClosed
^^^^^^^^^

Sets the door/window handle to closed position.

.. code-block:: arduino

    bool setClosed();

This function will return ``true`` if successful, ``false`` otherwise.

setOpen
^^^^^^^

Sets the door/window handle to open position.

.. code-block:: arduino

    bool setOpen();

This function will return ``true`` if successful, ``false`` otherwise.

setTilted
^^^^^^^^^

Sets the door/window handle to tilted position.

.. code-block:: arduino

    bool setTilted();

This function will return ``true`` if successful, ``false`` otherwise.

setIASClientEndpoint
^^^^^^^^^^^^^^^^^^^^

Sets the IAS Client endpoint number (default is 1).

.. code-block:: arduino

    void setIASClientEndpoint(uint8_t ep_number);

* ``ep_number`` - IAS Client endpoint number

report
^^^^^^

Manually reports the current handle position.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

*To be added*
