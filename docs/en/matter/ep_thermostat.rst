################
MatterThermostat
################

About
-----

The ``MatterThermostat`` class provides a thermostat endpoint for Matter networks with temperature control, setpoints, and multiple operating modes. This endpoint implements the Matter thermostat standard.

**Features:**
* Multiple operating modes (OFF, HEAT, COOL, AUTO, etc.)
* Heating and cooling setpoint control
* Local temperature reporting
* Automatic temperature regulation
* Deadband control for AUTO mode
* Callback support for mode, temperature, and setpoint changes
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Use Cases:**
* HVAC systems
* Smart thermostats
* Temperature control systems
* Climate control automation
* Energy management systems

API Reference
-------------

Constructor
***********

MatterThermostat
^^^^^^^^^^^^^^^^

Creates a new Matter thermostat endpoint.

.. code-block:: arduino

    MatterThermostat();

Initialization
**************

begin
^^^^^

Initializes the Matter thermostat endpoint with control sequence and auto mode settings.

.. code-block:: arduino

    bool begin(ControlSequenceOfOperation_t controlSequence = THERMOSTAT_SEQ_OP_COOLING, ThermostatAutoMode_t autoMode = THERMOSTAT_AUTO_MODE_DISABLED);

* ``controlSequence`` - Control sequence of operation (default: ``THERMOSTAT_SEQ_OP_COOLING``)
* ``autoMode`` - Auto mode enabled/disabled (default: ``THERMOSTAT_AUTO_MODE_DISABLED``)

This function will return ``true`` if successful, ``false`` otherwise.

end
^^^

Stops processing Matter thermostat events.

.. code-block:: arduino

    void end();

Control Sequences
*****************

ControlSequenceOfOperation_t
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Control sequence enumeration:

* ``THERMOSTAT_SEQ_OP_COOLING`` - Cooling only
* ``THERMOSTAT_SEQ_OP_COOLING_REHEAT`` - Cooling with reheat
* ``THERMOSTAT_SEQ_OP_HEATING`` - Heating only
* ``THERMOSTAT_SEQ_OP_HEATING_REHEAT`` - Heating with reheat
* ``THERMOSTAT_SEQ_OP_COOLING_HEATING`` - Cooling and heating
* ``THERMOSTAT_SEQ_OP_COOLING_HEATING_REHEAT`` - Cooling and heating with reheat

Thermostat Modes
****************

ThermostatMode_t
^^^^^^^^^^^^^^^^

Thermostat mode enumeration:

* ``THERMOSTAT_MODE_OFF`` - Off
* ``THERMOSTAT_MODE_AUTO`` - Auto mode
* ``THERMOSTAT_MODE_COOL`` - Cooling mode
* ``THERMOSTAT_MODE_HEAT`` - Heating mode
* ``THERMOSTAT_MODE_EMERGENCY_HEAT`` - Emergency heat
* ``THERMOSTAT_MODE_PRECOOLING`` - Precooling
* ``THERMOSTAT_MODE_FAN_ONLY`` - Fan only
* ``THERMOSTAT_MODE_DRY`` - Dry mode
* ``THERMOSTAT_MODE_SLEEP`` - Sleep mode

Mode Control
************

setMode
^^^^^^^

Sets the thermostat mode.

.. code-block:: arduino

    bool setMode(ThermostatMode_t mode);

getMode
^^^^^^^

Gets the current thermostat mode.

.. code-block:: arduino

    ThermostatMode_t getMode();

getThermostatModeString
^^^^^^^^^^^^^^^^^^^^^^^

Gets a friendly string for the thermostat mode.

.. code-block:: arduino

    static const char *getThermostatModeString(uint8_t mode);

Temperature Control
*******************

setLocalTemperature
^^^^^^^^^^^^^^^^^^^

Sets the local temperature reading.

.. code-block:: arduino

    bool setLocalTemperature(double temperature);

* ``temperature`` - Temperature in Celsius

getLocalTemperature
^^^^^^^^^^^^^^^^^^^

Gets the current local temperature.

.. code-block:: arduino

    double getLocalTemperature();

Setpoint Control
****************

setCoolingHeatingSetpoints
^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets both cooling and heating setpoints.

.. code-block:: arduino

    bool setCoolingHeatingSetpoints(double _setpointHeatingTemperature, double _setpointCoolingTemperature);

* ``_setpointHeatingTemperature`` - Heating setpoint in Celsius (or 0xffff to keep current)
* ``_setpointCoolingTemperature`` - Cooling setpoint in Celsius (or 0xffff to keep current)

**Note:** Heating setpoint must be lower than cooling setpoint. In AUTO mode, cooling setpoint must be at least 2.5°C higher than heating setpoint (deadband).

setHeatingSetpoint
^^^^^^^^^^^^^^^^^^

Sets the heating setpoint.

.. code-block:: arduino

    bool setHeatingSetpoint(double _setpointHeatingTemperature);

getHeatingSetpoint
^^^^^^^^^^^^^^^^^^

Gets the heating setpoint.

.. code-block:: arduino

    double getHeatingSetpoint();

setCoolingSetpoint
^^^^^^^^^^^^^^^^^^

Sets the cooling setpoint.

.. code-block:: arduino

    bool setCoolingSetpoint(double _setpointCoolingTemperature);

getCoolingSetpoint
^^^^^^^^^^^^^^^^^^

Gets the cooling setpoint.

.. code-block:: arduino

    double getCoolingSetpoint();

Setpoint Limits
***************

getMinHeatSetpoint
^^^^^^^^^^^^^^^^^^

Gets the minimum heating setpoint limit.

.. code-block:: arduino

    float getMinHeatSetpoint();

getMaxHeatSetpoint
^^^^^^^^^^^^^^^^^^

Gets the maximum heating setpoint limit.

.. code-block:: arduino

    float getMaxHeatSetpoint();

getMinCoolSetpoint
^^^^^^^^^^^^^^^^^^

Gets the minimum cooling setpoint limit.

.. code-block:: arduino

    float getMinCoolSetpoint();

getMaxCoolSetpoint
^^^^^^^^^^^^^^^^^^

Gets the maximum cooling setpoint limit.

.. code-block:: arduino

    float getMaxCoolSetpoint();

getDeadBand
^^^^^^^^^^^

Gets the deadband value (minimum difference between heating and cooling setpoints in AUTO mode).

.. code-block:: arduino

    float getDeadBand();

Event Handling
**************

onChange
^^^^^^^^

Sets a callback for when any parameter changes.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

onChangeMode
^^^^^^^^^^^^

Sets a callback for mode changes.

.. code-block:: arduino

    void onChangeMode(EndPointModeCB onChangeCB);

onChangeLocalTemperature
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback for local temperature changes.

.. code-block:: arduino

    void onChangeLocalTemperature(EndPointTemperatureCB onChangeCB);

onChangeCoolingSetpoint
^^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback for cooling setpoint changes.

.. code-block:: arduino

    void onChangeCoolingSetpoint(EndPointCoolingSetpointCB onChangeCB);

onChangeHeatingSetpoint
^^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback for heating setpoint changes.

.. code-block:: arduino

    void onChangeHeatingSetpoint(EndPointHeatingSetpointCB onChangeCB);

Example
-------

Thermostat
**********

.. literalinclude:: ../../../libraries/Matter/examples/MatterThermostat/MatterThermostat.ino
    :language: arduino

