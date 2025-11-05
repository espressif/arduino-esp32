####################
MatterContactSensor
####################

About
-----

The ``MatterContactSensor`` class provides a contact sensor endpoint for Matter networks. This endpoint implements the Matter contact sensing standard for detecting open/closed states (e.g., doors, windows).

**Features:**
* Contact state reporting (open/closed)
* Simple boolean state
* Read-only sensor (no control functionality)
* Automatic state updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Door/window sensors
* Contact switches
* Security systems
* Access control
* Smart home automation triggers

API Reference
-------------

Constructor
***********

MatterContactSensor
^^^^^^^^^^^^^^^^^^^

Creates a new Matter contact sensor endpoint.

.. code-block:: arduino

    MatterContactSensor();

Initialization
**************

begin
^^^^^

Initializes the Matter contact sensor endpoint with an initial contact state.

.. code-block:: arduino

    bool begin(bool _contactState = false);

* ``_contactState`` - Initial contact state (``true`` = closed, ``false`` = open, default: ``false``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter contact sensor events.

.. code-block:: arduino

    void end();

Contact State Control
*********************

setContact
^^^^^^^^^^

Sets the contact state.

.. code-block:: arduino

    bool setContact(bool _contactState);

* ``_contactState`` - Contact state (``true`` = closed, ``false`` = open)

This function will return ``true`` if successful, ``false`` otherwise.

getContact
^^^^^^^^^^

Gets the current contact state.

.. code-block:: arduino

    bool getContact();

This function will return ``true`` if closed, ``false`` if open.

Operators
*********

bool operator
^^^^^^^^^^^^^

Returns the current contact state.

.. code-block:: arduino

    operator bool();

Example:

.. code-block:: arduino

    if (mySensor) {
        Serial.println("Contact is closed");
    } else {
        Serial.println("Contact is open");
    }

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the contact state.

.. code-block:: arduino

    void operator=(bool _contactState);

Example:

.. code-block:: arduino

    mySensor = true;   // Set contact to closed
    mySensor = false;  // Set contact to open

Example
-------

Contact Sensor
**************

.. literalinclude:: ../../../libraries/Matter/examples/MatterContactSensor/MatterContactSensor.ino
    :language: arduino

