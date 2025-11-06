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

report
^^^^^^

Manually reports the current handle position.

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

This function returns ``true`` if the device is enrolled, ``false`` otherwise. Use this method to check the enrollment status after calling ``requestIASZoneEnroll()``.

Example
-------

*To be added*
