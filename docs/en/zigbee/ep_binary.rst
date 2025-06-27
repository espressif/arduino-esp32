############
ZigbeeBinary
############

About
-----

The ``ZigbeeBinary`` class provides an endpoint for binary input/output sensors in Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for binary sensors, supporting various application types for HVAC, security, and general binary sensing.
Binary Input (BI) is meant to be used for sensors that provide a binary signal, such as door/window sensors, motion detectors, etc. to be sent to the network.

.. note::

    Binary Output (BO) is not supported yet.

API Reference
-------------

Constructor
***********

ZigbeeBinary
^^^^^^^^^^^^

Creates a new Zigbee binary sensor endpoint.

.. code-block:: arduino

    ZigbeeBinary(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Binary Input Application Types
******************************

HVAC Application Types
^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_BOILER_STATUS  0x00000003
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_CHILLER_STATUS 0x00000013
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_OCCUPANCY      0x00000031
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_FAN_STATUS     0x00000035
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_FILTER_STATUS  0x00000036
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_HEATING_ALARM  0x0000003E
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_COOLING_ALARM  0x0000001D
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_UNIT_ENABLE    0x00000090
    #define BINARY_INPUT_APPLICATION_TYPE_HVAC_OTHER          0x0000FFFF

Security Application Types
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_GLASS_BREAKAGE_DETECTION_0 0x01000000
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_INTRUSION_DETECTION        0x01000001
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_MOTION_DETECTION           0x01000002
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_GLASS_BREAKAGE_DETECTION_1 0x01000003
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_ZONE_ARMED                 0x01000004
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_GLASS_BREAKAGE_DETECTION_2 0x01000005
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_SMOKE_DETECTION            0x01000006
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_CARBON_DIOXIDE_DETECTION   0x01000007
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_HEAT_DETECTION             0x01000008
    #define BINARY_INPUT_APPLICATION_TYPE_SECURITY_OTHER                      0x0100FFFF

API Methods
***********

addBinaryInput
^^^^^^^^^^^^^^

Adds a binary input cluster to the endpoint.

.. code-block:: arduino

    bool addBinaryInput();

This function will return ``true`` if successful, ``false`` otherwise.

setBinaryInputApplication
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the application type for the binary input.

.. code-block:: arduino

    bool setBinaryInputApplication(uint32_t application_type);

* ``application_type`` - Application type constant (see above)

This function will return ``true`` if successful, ``false`` otherwise.

setBinaryInputDescription
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a custom description for the binary input.

.. code-block:: arduino

    bool setBinaryInputDescription(const char *description);

* ``description`` - Description string

This function will return ``true`` if successful, ``false`` otherwise.

setBinaryInput
^^^^^^^^^^^^^^

Sets the binary input value.

.. code-block:: arduino

    bool setBinaryInput(bool input);

* ``input`` - Binary value (true/false)

This function will return ``true`` if successful, ``false`` otherwise.

reportBinaryInput
^^^^^^^^^^^^^^^^^

Manually reports the current binary input value.

.. code-block:: arduino

    bool reportBinaryInput();

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Binary Input Implementation
****************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Binary_Input/Zigbee_Binary_Input.ino
    :language: arduino
