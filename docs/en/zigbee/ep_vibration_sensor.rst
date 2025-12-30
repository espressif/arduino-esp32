#####################
ZigbeeVibrationSensor
#####################

About
-----

The ``ZigbeeVibrationSensor`` class provides a vibration sensor endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for vibration detection devices.

**Features:**
* Vibration detection and measurement
* Configurable sensitivity levels
* Multiple detection modes
* Automatic reporting capabilities
* Integration with common endpoint features (binding, OTA, etc.)
* Zigbee HA standard compliance

**Use Cases:**
* Security system vibration detection
* Industrial equipment monitoring
* Structural health monitoring
* Smart home security applications
* Machine condition monitoring

API Reference
-------------

Constructor
***********

ZigbeeVibrationSensor
^^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee vibration sensor endpoint.

.. code-block:: arduino

    ZigbeeVibrationSensor(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

API Methods
***********

setVibration
^^^^^^^^^^^^

Sets the vibration detection state.

.. code-block:: arduino

    bool setVibration(bool sensed);

* ``sensed`` - Vibration state (true = sensed, false = not sensed)

This function will return ``true`` if successful, ``false`` otherwise.

setIASClientEndpoint
^^^^^^^^^^^^^^^^^^^^

Sets the IAS Client endpoint number (default is 1).

.. code-block:: arduino

    void setIASClientEndpoint(uint8_t ep_number);

* ``ep_number`` - IAS Client endpoint number

report
^^^^^^

Manually reports the current vibration state.

.. code-block:: arduino

    bool report();

This function will return ``true`` if successful, ``false`` otherwise.

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

Vibration Sensor Implementation
*******************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Vibration_Sensor/Zigbee_Vibration_Sensor.ino
    :language: arduino
