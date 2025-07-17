############
ZigbeeAnalog
############

About
-----

The ``ZigbeeAnalog`` class provides analog input and output endpoints for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for analog signal processing and control.
Analog Input (AI) is meant to be used for sensors that provide an analog signal, such as temperature, humidity, pressure to be sent to the coordinator.
Analog Output (AO) is meant to be used for actuators that require an analog signal, such as dimmers, valves, etc. to be controlled by the coordinator.


Common API
----------

Constructor
***********

ZigbeeAnalog
^^^^^^^^^^^^

Creates a new Zigbee analog endpoint.

.. code-block:: arduino

    ZigbeeAnalog(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Cluster Management
******************

addAnalogInput
^^^^^^^^^^^^^^

Adds analog input cluster to the endpoint.

.. code-block:: arduino

    bool addAnalogInput();

This function will return ``true`` if successful, ``false`` otherwise.

addAnalogOutput
^^^^^^^^^^^^^^^

Adds analog output cluster to the endpoint.

.. code-block:: arduino

    bool addAnalogOutput();

This function will return ``true`` if successful, ``false`` otherwise.

Analog Input API
----------------

Configuration Methods
*********************

setAnalogInputApplication
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the application type for the analog input.

.. code-block:: arduino

    bool setAnalogInputApplication(uint32_t application_type);

* ``application_type`` - Application type constant (see esp_zigbee_zcl_analog_input.h for values)

This function will return ``true`` if successful, ``false`` otherwise.

setAnalogInputDescription
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a custom description for the analog input.

.. code-block:: arduino

    bool setAnalogInputDescription(const char *description);

* ``description`` - Description string

This function will return ``true`` if successful, ``false`` otherwise.

setAnalogInputResolution
^^^^^^^^^^^^^^^^^^^^^^^^

Sets the resolution for the analog input.

.. code-block:: arduino

    bool setAnalogInputResolution(float resolution);

* ``resolution`` - Resolution value

This function will return ``true`` if successful, ``false`` otherwise.

setAnalogInputMinMax
^^^^^^^^^^^^^^^^^^^^

Sets the minimum and maximum values for the analog input.

.. code-block:: arduino

    bool setAnalogInputMinMax(float min, float max);

* ``min`` - Minimum value
* ``max`` - Maximum value

This function will return ``true`` if successful, ``false`` otherwise.

Value Control
*************

setAnalogInput
^^^^^^^^^^^^^^

Sets the analog input value.

.. code-block:: arduino

    bool setAnalogInput(float analog);

* ``analog`` - Analog input value

This function will return ``true`` if successful, ``false`` otherwise.

Reporting Methods
*****************

setAnalogInputReporting
^^^^^^^^^^^^^^^^^^^^^^^

Sets the reporting configuration for analog input.

.. code-block:: arduino

    bool setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta);

* ``min_interval`` - Minimum reporting interval in seconds
* ``max_interval`` - Maximum reporting interval in seconds
* ``delta`` - Minimum change in value to trigger a report

This function will return ``true`` if successful, ``false`` otherwise.

reportAnalogInput
^^^^^^^^^^^^^^^^^

Manually reports the current analog input value.

.. code-block:: arduino

    bool reportAnalogInput();

This function will return ``true`` if successful, ``false`` otherwise.

Analog Output API
-----------------

Configuration Methods
*********************

setAnalogOutputApplication
^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the application type for the analog output.

.. code-block:: arduino

    bool setAnalogOutputApplication(uint32_t application_type);

* ``application_type`` - Application type constant (see esp_zigbee_zcl_analog_output.h for values)

This function will return ``true`` if successful, ``false`` otherwise.

setAnalogOutputDescription
^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a custom description for the analog output.

.. code-block:: arduino

    bool setAnalogOutputDescription(const char *description);

* ``description`` - Description string

This function will return ``true`` if successful, ``false`` otherwise.

setAnalogOutputResolution
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the resolution for the analog output.

.. code-block:: arduino

    bool setAnalogOutputResolution(float resolution);

* ``resolution`` - Resolution value

This function will return ``true`` if successful, ``false`` otherwise.

setAnalogOutputMinMax
^^^^^^^^^^^^^^^^^^^^^

Sets the minimum and maximum values for the analog output.

.. code-block:: arduino

    bool setAnalogOutputMinMax(float min, float max);

* ``min`` - Minimum value
* ``max`` - Maximum value

This function will return ``true`` if successful, ``false`` otherwise.

Value Control
*************

setAnalogOutput
^^^^^^^^^^^^^^^

Sets the analog output value.

.. code-block:: arduino

    bool setAnalogOutput(float analog);

* ``analog`` - Analog output value

This function will return ``true`` if successful, ``false`` otherwise.

getAnalogOutput
^^^^^^^^^^^^^^^

Gets the current analog output value.

.. code-block:: arduino

    float getAnalogOutput();

This function will return current analog output value.

Reporting Methods
*****************

reportAnalogOutput
^^^^^^^^^^^^^^^^^^

Manually reports the current analog output value.

.. code-block:: arduino

    bool reportAnalogOutput();

This function will return ``true`` if successful, ``false`` otherwise.

Event Handling
**************

onAnalogOutputChange
^^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when the analog output value changes.

.. code-block:: arduino

    void onAnalogOutputChange(void (*callback)(float analog));

* ``callback`` - Function to call when analog output changes

Example
-------

Analog Input/Output
*******************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Analog_Input_Output/Zigbee_Analog_Input_Output.ino
    :language: arduino
