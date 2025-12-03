##########################
MatterWaterLeakDetector
##########################

About
-----

The ``MatterWaterLeakDetector`` class provides a water leak detector endpoint for Matter networks. This endpoint implements the Matter water leak detection standard for detecting water leak conditions (detected/not detected states).

**Features:**
* Water leak detection state reporting (detected/not detected)
* Simple boolean state
* Read-only sensor (no control functionality)
* Automatic state updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Water leak monitoring
* Basement flood detection
* Appliance leak detection
* Smart home automation triggers
* Preventative maintenance systems

API Reference
-------------

Constructor
***********

MatterWaterLeakDetector
^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter water leak detector endpoint.

.. code-block:: arduino

    MatterWaterLeakDetector();

Initialization
**************

begin
^^^^^

Initializes the Matter water leak detector endpoint with an initial leak detection state.

.. code-block:: arduino

    bool begin(bool _leakState = false);

* ``_leakState`` - Initial water leak detection state (``true`` = detected, ``false`` = not detected, default: ``false``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter water leak detector events.

.. code-block:: arduino

    void end();

Water Leak Detection State Control
***********************************

setLeak
^^^^^^^^

Sets the water leak detection state.

.. code-block:: arduino

    bool setLeak(bool _leakState);

* ``_leakState`` - Water leak detection state (``true`` = detected, ``false`` = not detected)

This function will return ``true`` if successful, ``false`` otherwise.

getLeak
^^^^^^^

Gets the current water leak detection state.

.. code-block:: arduino

    bool getLeak();

This function will return ``true`` if water leak is detected, ``false`` if not detected.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current water leak detection state.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (myDetector) {
        Serial.println("Water leak is detected");
    } else {
        Serial.println("Water leak is not detected");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the water leak detection state.

.. code-block:: arduino

    void operator=(bool _leakState);

Example:

.. code-block:: arduino

    myDetector = true;   // Set water leak detection to detected
    myDetector = false;  // Set water leak detection to not detected

Example
-------

Water Leak Detector
********************

.. literalinclude:: ../../../libraries/Matter/examples/MatterWaterLeakDetector/MatterWaterLeakDetector.ino
    :language: arduino

