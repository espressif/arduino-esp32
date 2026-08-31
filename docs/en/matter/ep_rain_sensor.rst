###################
MatterRainSensor
###################

About
-----

The ``MatterRainSensor`` class provides a rain sensor endpoint for Matter networks. This endpoint implements the Matter rain sensing standard for detecting rain presence (detected/not detected states).

**Features:**
* Rain detection state reporting (detected/not detected)
* Simple boolean state
* Read-only sensor (no control functionality)
* Automatic state updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Weather monitoring systems
* Irrigation control systems
* Outdoor sensor networks
* Smart home automation triggers
* Rain detection for automated systems

API Reference
-------------

Constructor
***********

MatterRainSensor
^^^^^^^^^^^^^^^^

Creates a new Matter rain sensor endpoint.

.. code-block:: arduino

    MatterRainSensor();

Initialization
**************

begin
^^^^^

Initializes the Matter rain sensor endpoint. Fabric ``StateValue`` starts ``false`` (not detected). Call ``setRain()`` after ``Matter.begin()`` with the real sensor reading.

.. code-block:: arduino

    bool begin();

This function will return ``true`` if successful, ``false`` otherwise.

Typical usage:

.. code-block:: arduino

    RainSensor.begin();
    Matter.begin();
    RainSensor.setRain(digitalRead(rainPin));

end
^^^

Stops processing Matter rain sensor events.

.. code-block:: arduino

    void end();

Rain Detection State Control
****************************

setRain
^^^^^^^

Sets the rain detection state. Call after ``Matter.begin()``.

.. code-block:: arduino

    bool setRain(bool _rainState);

* ``_rainState`` - Rain detection state (``true`` = detected, ``false`` = not detected)

This function will return ``true`` if successful, ``false`` otherwise.

getRain
^^^^^^^

Gets the current rain detection state.

.. code-block:: arduino

    bool getRain();

This function will return ``true`` if rain is detected, ``false`` if not detected.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current rain detection state.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (mySensor) {
        Serial.println("Rain is detected");
    } else {
        Serial.println("Rain is not detected");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the rain detection state. Same as ``setRain()``; call after ``Matter.begin()``.

.. code-block:: arduino

    void operator=(bool _rainState);

Example:

.. code-block:: arduino

    mySensor = true;   // Set rain detection to detected
    mySensor = false;  // Set rain detection to not detected

Example
-------

Rain Sensor
***********

.. literalinclude:: ../../../libraries/Matter/examples/Sensors/MatterRainSensor/MatterRainSensor.ino
    :language: arduino
