###################
ZigbeeContactSwitch
###################

About
-----

The ``ZigbeeContactSwitch`` class provides a contact switch endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for door/window contact sensors and other binary contact devices.

**Features:**
* Contact state detection (open/closed)
* Configurable application types
* Automatic reporting capabilities
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Door and window sensors
* Security system contacts
* Cabinet and drawer sensors
* Industrial contact monitoring
* Smart home security applications

API Reference
-------------

Constructor
***********

ZigbeeContactSwitch
^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee contact switch endpoint.

.. code-block:: arduino

    ZigbeeContactSwitch(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setClosed
^^^^^^^^^

Sets the contact switch to closed state.

.. code-block:: arduino

    bool setClosed();

This function will return ``true`` if successful, ``false`` otherwise.

setOpen
^^^^^^^

Sets the contact switch to open state.

.. code-block:: arduino

    bool setOpen();

This function will return ``true`` if successful, ``false`` otherwise.

report
^^^^^^

Manually reports the current contact state.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

setIASClientEndpoint
^^^^^^^^^^^^^^^^^^^^

Sets the IAS Client endpoint number (default is 1).

.. code-block:: arduino

    void setIASClientEndpoint(uint8_t ep_number);

* ``ep_number`` - IAS Client endpoint number

requestIASZoneEnroll
^^^^^^^^^^^^^^^^^^^^

Requests a new IAS Zone enrollment. Can be called to enroll a new device or to re-enroll an already enrolled device.

.. code-block:: arduino

    bool requestIASZoneEnroll();

This function will return ``true`` if the enrollment request was sent successfully, ``false`` otherwise. The actual enrollment status should be checked using the ``enrolled()`` method after waiting for the enrollment response.

restoreIASZoneEnroll
^^^^^^^^^^^^^^^^^^^^

Restores IAS Zone enrollment from stored attributes. This method should be called after rebooting an already enrolled device. It restores the enrollment information from flash memory, which is faster for sleepy devices compared to requesting a new enrollment.

.. code-block:: arduino

    bool restoreIASZoneEnroll();

This function will return ``true`` if the enrollment was successfully restored, ``false`` otherwise. The enrollment information (zone ID and IAS CIE address) must be available in the device's stored attributes for this to succeed.

enrolled
^^^^^^^^

Checks if the device is currently enrolled in the IAS Zone.

.. code-block:: arduino

    bool enrolled();

This function returns ``true`` if the device is enrolled, ``false`` otherwise. Use this method to check the enrollment status after calling ``requestIASZoneEnroll()`` or ``restoreIASZoneEnroll()``.

Example
-------

Contact Switch Implementation
*****************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Contact_Switch/Zigbee_Contact_Switch.ino
    :language: arduino
