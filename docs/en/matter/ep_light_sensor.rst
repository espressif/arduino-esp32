#################
MatterLightSensor
#################

About
-----

The ``MatterLightSensor`` class provides an illuminance sensor endpoint for Matter networks. This endpoint implements the Matter illuminance sensing standard for read-only illuminance reporting.

**Features:**
* Illuminance measurement reporting (1lx-3.576Mlx)
* Read-only sensor (no control functionality)
* Automatic illuminance updates
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* Room illuminance monitoring
* Weather stations
* Illuminance logging
* Smart home climate monitoring

API Reference
-------------

Constructor
***********

MatterLightSensor
^^^^^^^^^^^^^^^^^

Creates a new Matter light sensor endpoint.

.. code-block:: arduino

    MatterLightSensor();

Initialization
**************

begin
^^^^^

Initializes the Matter light sensor endpoint with an initial illuminance value.

.. code-block:: arduino

    bool begin(double illuminance = 1.00)

* ``illuminance`` - Initial illuminance value (1lx-3.576Mlx, default: 1.00)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter light sensor events.

.. code-block:: arduino

    void end();

Illuminance Control
*******************

setIlluminance
^^^^^^^^^^^^^^

Sets the reported illuminance value.

.. code-block:: arduino

    bool setIlluminance(double illuminance);

* ``illuminance`` - Initial illuminance value (1lx-3.576Mlx, default: 1.00)

This function will return ``true`` if successful, ``false`` otherwise.

getIlluminance
^^^^^^^^^^^^^^

Gets the current reported illuminance value.

.. code-block:: arduino

    double getIlluminance();

This function will return the illuminance value.

Operators
*********

double operator
^^^^^^^^^^^^^^^

Returns the current illuminance value.

.. code-block:: arduino

    operator double();

Example:

.. code-block:: arduino

    double illuminance = mySensor;  // Get current illuminance

Assignment operator
^^^^^^^^^^^^^^^^^^^

Sets the illuminance value.

.. code-block:: arduino

    void operator=(double illuminance);

Example:

.. code-block:: arduino

    mySensor = 45.5;  // Set illuminance to 45.5lx

Example
-------

Light Sensor
************

.. literalinclude:: ../../../libraries/Matter/examples/MatterLightSensor/MatterLightSensor.ino
    :language: arduino
