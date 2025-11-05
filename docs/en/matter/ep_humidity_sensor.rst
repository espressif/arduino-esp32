#####################
MatterHumiditySensor
#####################

About
-----

The ``MatterHumiditySensor`` class provides a humidity sensor endpoint for Matter networks. This endpoint implements the Matter humidity sensing standard for read-only humidity reporting.

**Features:**
* Humidity measurement reporting (0-100%)
* 1/100th percent precision
* Read-only sensor (no control functionality)
* Automatic humidity updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Room humidity monitoring
* Weather stations
* HVAC systems
* Humidity logging
* Smart home climate monitoring

API Reference
-------------

Constructor
***********

MatterHumiditySensor
^^^^^^^^^^^^^^^^^^^^

Creates a new Matter humidity sensor endpoint.

.. code-block:: arduino

    MatterHumiditySensor();

Initialization
**************

begin
^^^^^

Initializes the Matter humidity sensor endpoint with an initial humidity value.

.. code-block:: arduino

    bool begin(double humidityPercent = 0.00);

* ``humidityPercent`` - Initial humidity percentage (0.0-100.0, default: 0.00)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** The implementation stores humidity with 1/100th percent precision internally. Values must be between 0.0 and 100.0.

end
^^^

Stops processing Matter humidity sensor events.

.. code-block:: arduino

    void end();

Humidity Control
***************

setHumidity
^^^^^^^^^^^

Sets the reported humidity percentage.

.. code-block:: arduino

    bool setHumidity(double humidityPercent);

* ``humidityPercent`` - Humidity percentage (0.0-100.0)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Humidity is stored with 1/100th percent precision. Values outside the 0.0-100.0 range will be rejected.

getHumidity
^^^^^^^^^^^

Gets the current reported humidity percentage.

.. code-block:: arduino

    double getHumidity();

This function will return the humidity percentage with 1/100th percent precision.

Operators
*********

double operator
^^^^^^^^^^^^^^^

Returns the current humidity percentage.

.. code-block:: arduino

    operator double();

Example:

.. code-block:: arduino

    double humidity = mySensor;  // Get current humidity

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the humidity percentage.

.. code-block:: arduino

    void operator=(double humidityPercent);

Example:

.. code-block:: arduino

    mySensor = 45.5;  // Set humidity to 45.5%

Example
-------

Humidity Sensor
***************

.. literalinclude:: ../../../libraries/Matter/examples/MatterHumiditySensor/MatterHumiditySensor.ino
    :language: arduino

