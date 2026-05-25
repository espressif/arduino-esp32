#######################
MatterTemperatureSensor
#######################

About
-----

The ``MatterTemperatureSensor`` class provides a temperature sensor endpoint for Matter networks. This endpoint implements the Matter temperature sensing standard for read-only temperature reporting.

**Features:**
* Temperature measurement reporting in Celsius
* 1/100th degree Celsius precision
* Read-only sensor (no control functionality)
* Automatic temperature updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Room temperature monitoring
* Weather stations
* HVAC systems
* Temperature logging
* Smart home climate monitoring

API Reference
-------------

Constructor
***********

MatterTemperatureSensor
^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter temperature sensor endpoint.

.. code-block:: arduino

    MatterTemperatureSensor();

Initialization
**************

begin
^^^^^

Initializes the Matter temperature sensor endpoint with an initial temperature value.

.. code-block:: arduino

    bool begin(double temperature = 0.00);

* ``temperature`` - Initial temperature in Celsius (default: 0.00)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** The implementation stores temperature with 1/100th degree Celsius precision internally.

end
^^^

Stops processing Matter temperature sensor events.

.. code-block:: arduino

    void end();

Temperature Control
*******************

setTemperature
^^^^^^^^^^^^^^

Sets the reported temperature value.

.. code-block:: arduino

    bool setTemperature(double temperature);

* ``temperature`` - Temperature in Celsius

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Temperature is stored with 1/100th degree Celsius precision. The valid range is typically -273.15°C (absolute zero) to 327.67°C.

getTemperature
^^^^^^^^^^^^^^

Gets the current reported temperature value.

.. code-block:: arduino

    double getTemperature();

This function will return the temperature in Celsius with 1/100th degree precision.

Operators
*********

double operator
^^^^^^^^^^^^^^^

Returns the current temperature.

.. code-block:: arduino

    operator double();

Example:

.. code-block:: arduino

    double temp = mySensor;  // Get current temperature

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the temperature value.

.. code-block:: arduino

    void operator=(double temperature);

Example:

.. code-block:: arduino

    mySensor = 25.5;  // Set temperature to 25.5°C

Example
-------

Temperature Sensor
******************

.. literalinclude:: ../../../libraries/Matter/examples/MatterTemperatureSensor/MatterTemperatureSensor.ino
    :language: arduino
