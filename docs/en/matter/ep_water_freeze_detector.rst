###########################
MatterWaterFreezeDetector
###########################

About
-----

The ``MatterWaterFreezeDetector`` class provides a water freeze detector endpoint for Matter networks. This endpoint implements the Matter water freeze detection standard for detecting water freeze conditions (detected/not detected states).

**Features:**
* Water freeze detection state reporting (detected/not detected)
* Simple boolean state
* Read-only sensor (no control functionality)
* Automatic state updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Water pipe freeze monitoring
* Outdoor water system protection
* HVAC freeze detection
* Smart home automation triggers
* Preventative maintenance systems

API Reference
-------------

Constructor
***********

MatterWaterFreezeDetector
^^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter water freeze detector endpoint.

.. code-block:: arduino

    MatterWaterFreezeDetector();

Initialization
**************

begin
^^^^^

Initializes the Matter water freeze detector endpoint with an initial freeze detection state.

.. code-block:: arduino

    bool begin(bool _freezeState = false);

* ``_freezeState`` - Initial water freeze detection state (``true`` = detected, ``false`` = not detected, default: ``false``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter water freeze detector events.

.. code-block:: arduino

    void end();

Water Freeze Detection State Control
************************************

setFreeze
^^^^^^^^^

Sets the water freeze detection state.

.. code-block:: arduino

    bool setFreeze(bool _freezeState);

* ``_freezeState`` - Water freeze detection state (``true`` = detected, ``false`` = not detected)

This function will return ``true`` if successful, ``false`` otherwise.

getFreeze
^^^^^^^^^

Gets the current water freeze detection state.

.. code-block:: arduino

    bool getFreeze();

This function will return ``true`` if water freeze is detected, ``false`` if not detected.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current water freeze detection state.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (myDetector) {
        Serial.println("Water freeze is detected");
    } else {
        Serial.println("Water freeze is not detected");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the water freeze detection state.

.. code-block:: arduino

    void operator=(bool _freezeState);

Example:

.. code-block:: arduino

    myDetector = true;   // Set water freeze detection to detected
    myDetector = false;  // Set water freeze detection to not detected

Example
-------

Water Freeze Detector
*********************

.. literalinclude:: ../../../libraries/Matter/examples/MatterWaterFreezeDetector/MatterWaterFreezeDetector.ino
    :language: arduino

