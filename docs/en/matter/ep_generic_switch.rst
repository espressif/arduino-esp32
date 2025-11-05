####################
MatterGenericSwitch
####################

About
-----

The ``MatterGenericSwitch`` class provides a generic switch endpoint for Matter networks. This endpoint works as a smart button that can send click events to the Matter controller, enabling automation triggers.

**Features:**
* Click event reporting to Matter controller
* Simple button functionality
* Automation trigger support
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Smart buttons
* Automation triggers
* Remote controls
* Scene activation buttons
* Event generators for smart home automation

API Reference
-------------

Constructor
***********

MatterGenericSwitch
^^^^^^^^^^^^^^^^^^^

Creates a new Matter generic switch endpoint.

.. code-block:: arduino

    MatterGenericSwitch();

Initialization
**************

begin
^^^^^

Initializes the Matter generic switch endpoint.

.. code-block:: arduino

    bool begin();

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter switch events.

.. code-block:: arduino

    void end();

Event Generation
***************

click
^^^^^

Sends a click event to the Matter controller.

.. code-block:: arduino

    void click();

This function sends a click event that can be used to trigger automations in smart home apps. The event is sent immediately and the Matter controller will receive it.

**Usage Example:**

When a physical button is pressed and released, call this function to send the click event:

.. code-block:: arduino

    if (buttonReleased) {
        mySwitch.click();
        Serial.println("Click event sent to Matter controller");
    }

Example
-------

Generic Switch (Smart Button)
******************************

.. literalinclude:: ../../../libraries/Matter/examples/MatterSmartButon/MatterSmartButon.ino
    :language: arduino

