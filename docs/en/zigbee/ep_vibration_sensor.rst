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

    void report();

This function does not return a value.

Example
-------

Vibration Sensor Implementation
*******************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Vibration_Sensor/Zigbee_Vibration_Sensor.ino
    :language: arduino
