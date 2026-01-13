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

Initializes the Matter rain sensor endpoint with an initial rain detection state.

.. code-block:: arduino

    bool begin(bool _rainState = false);

* ``_rainState`` - Initial rain detection state (``true`` = detected, ``false`` = not detected, default: ``false``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter rain sensor events.

.. code-block:: arduino

    void end();

Rain Detection State Control
****************************

setRain
^^^^^^^

Sets the rain detection state.

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

Sets the rain detection state.

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

.. literalinclude:: ../../../libraries/Matter/examples/MatterRainSensor/MatterRainSensor.ino
    :language: arduino
