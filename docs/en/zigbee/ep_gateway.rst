#############
ZigbeeGateway
#############

About
-----

The ``ZigbeeGateway`` class provides a gateway endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for network coordination and gateway functionality.
Gateway is a device that can be used to bridge Zigbee network to other networks (e.g. Wi-Fi, Ethernet, etc.).

API Reference
-------------

Constructor
***********

ZigbeeGateway
^^^^^^^^^^^^^

Creates a new Zigbee gateway endpoint.

.. code-block:: arduino

    ZigbeeGateway(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Example
-------

Gateway Implementation
**********************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Gateway/Zigbee_Gateway.ino
    :language: arduino
