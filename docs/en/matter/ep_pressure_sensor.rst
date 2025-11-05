#####################
MatterPressureSensor
#####################

About
-----

The ``MatterPressureSensor`` class provides a pressure sensor endpoint for Matter networks. This endpoint implements the Matter pressure sensing standard for read-only pressure reporting.

**Features:**
* Pressure measurement reporting in hectopascals (hPa)
* Read-only sensor (no control functionality)
* Automatic pressure updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Atmospheric pressure monitoring
* Weather stations
* Barometric pressure sensors
* Pressure logging
* Smart home weather monitoring

API Reference
-------------

Constructor
***********

MatterPressureSensor
^^^^^^^^^^^^^^^^^^^^

Creates a new Matter pressure sensor endpoint.

.. code-block:: arduino

    MatterPressureSensor();

Initialization
**************

begin
^^^^^

Initializes the Matter pressure sensor endpoint with an initial pressure value.

.. code-block:: arduino

    bool begin(double pressure = 0.00);

* ``pressure`` - Initial pressure in hectopascals (hPa, default: 0.00)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Typical atmospheric pressure at sea level is around 1013.25 hPa. Pressure values typically range from 950-1050 hPa.

end
^^^

Stops processing Matter pressure sensor events.

.. code-block:: arduino

    void end();

Pressure Control
***************

setPressure
^^^^^^^^^^^

Sets the reported pressure value.

.. code-block:: arduino

    bool setPressure(double pressure);

* ``pressure`` - Pressure in hectopascals (hPa)

This function will return ``true`` if successful, ``false`` otherwise.

getPressure
^^^^^^^^^^^

Gets the current reported pressure value.

.. code-block:: arduino

    double getPressure();

This function will return the pressure in hectopascals (hPa).

Operators
*********

double operator
^^^^^^^^^^^^^^^

Returns the current pressure.

.. code-block:: arduino

    operator double();

Example:

.. code-block:: arduino

    double pressure = mySensor;  // Get current pressure

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the pressure value.

.. code-block:: arduino

    void operator=(double pressure);

Example:

.. code-block:: arduino

    mySensor = 1013.25;  // Set pressure to 1013.25 hPa

Example
-------

Pressure Sensor
***************

.. literalinclude:: ../../../libraries/Matter/examples/MatterPressureSensor/MatterPressureSensor.ino
    :language: arduino

