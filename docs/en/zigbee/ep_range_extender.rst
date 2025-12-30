###################
ZigbeeRangeExtender
###################

About
-----

The ``ZigbeeRangeExtender`` class provides a range extender endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for range extender devices that help extend the coverage of Zigbee networks by acting as repeaters.

API Reference
-------------

Constructor
***********

ZigbeeRangeExtender
^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee range extender endpoint.

.. code-block:: arduino

    ZigbeeRangeExtender(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Example
-------

Range Extender Implementation
*****************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Range_Extender/Zigbee_Range_Extender.ino
    :language: arduino
