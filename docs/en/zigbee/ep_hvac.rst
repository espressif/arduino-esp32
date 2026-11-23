##########
ZigbeeHVAC
##########

About
-----

The ``ZigbeeHVAC`` class is an HA thermostat server (Thermostat cluster ``0x0201``). It is the device a coordinator such as ZHA writes to. ``ZigbeeThermostat`` remains the coordinator client that receives temperature reports.

The constructor installs the attributes the stack does not create by itself: occupied heating and cooling setpoints (20.00°C and 26.00°C), ``MinSetpointDeadBand`` (2.5°C) and ``ThermostatRunningState``.

**Features:**

* System mode and control sequence
* Occupied heating and cooling setpoints, with the 2.5°C dead band
* Local temperature
* Running state
* Callback for attribute writes and ``SetpointRaiseOrLower``

Weekly schedule, setpoint setback and air-conditioner attributes are not in this class.

API Reference
-------------

Constructor
***********

ZigbeeHVAC
^^^^^^^^^^

.. code-block:: arduino

    ZigbeeHVAC(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Call the setters after ``Zigbee.role()``. They work before and after ``Zigbee.addEndpoint()``. Temperatures are degrees Celsius.

Call ``enableReporting()`` after ``Zigbee.role()`` and before ``Zigbee.begin()``. It makes running state reportable. The stack rejects that change once ``begin()`` has registered the device. ``LocalTemperature`` is already reportable. System mode and the occupied setpoints stay unreportable. The stack answers those writes and reports local temperature and running state to the bound coordinator.

.. code-block:: arduino

    bool enableReporting();

System Mode
***********

.. code-block:: arduino

    enum ZigbeeHVACSystemMode {
      HVAC_SYSTEM_MODE_OFF,
      HVAC_SYSTEM_MODE_AUTO,
      HVAC_SYSTEM_MODE_COOL,
      HVAC_SYSTEM_MODE_HEAT,
      HVAC_SYSTEM_MODE_EMERGENCY_HEAT,
      HVAC_SYSTEM_MODE_PRECOOLING,
      HVAC_SYSTEM_MODE_FAN_ONLY,
      HVAC_SYSTEM_MODE_DRY,
      HVAC_SYSTEM_MODE_SLEEP,
    };

``0x02`` is not a system mode. ``setSystemMode()`` rejects it.

Control Sequence
****************

.. code-block:: arduino

    enum ZigbeeHVACControlSequence {
      HVAC_SEQ_COOLING_ONLY,
      HVAC_SEQ_COOLING_WITH_REHEAT,
      HVAC_SEQ_HEATING_ONLY,
      HVAC_SEQ_HEATING_WITH_REHEAT,
      HVAC_SEQ_COOLING_AND_HEATING,
      HVAC_SEQ_COOLING_AND_HEATING_WITH_REHEAT,
    };

The default sequence is cooling and heating. Set the sequence that matches the equipment so the coordinator offers the right modes.

Setpoints
*********

.. code-block:: arduino

    bool setOccupiedHeatingSetpoint(float celsius);
    bool setOccupiedCoolingSetpoint(float celsius);
    bool setLocalTemperature(float celsius);

Occupied cooling must stay at least 2.5°C above occupied heating. Raise cooling before raising heating when both move up. ``setLocalTemperature()`` publishes the measured room temperature. Do not write the invalid value ``0x8000``.

Running State
*************

``ThermostatRunningState`` is a bitmap. Combine the bits with bitwise OR.

.. code-block:: arduino

    HVAC_RUNNING_OFF
    HVAC_RUNNING_HEAT
    HVAC_RUNNING_COOL
    HVAC_RUNNING_FAN
    HVAC_RUNNING_HEAT_2ND
    HVAC_RUNNING_COOL_2ND
    HVAC_RUNNING_FAN_2ND
    HVAC_RUNNING_FAN_3RD

.. code-block:: arduino

    bool setRunningState(uint16_t running_state);

Event Handling
**************

.. code-block:: arduino

    void onHVACChange(void (*callback)(ZigbeeHVACSystemMode mode, float heating_setpoint, float cooling_setpoint));

The callback runs when the coordinator writes system mode or an occupied setpoint, and when it sends ``SetpointRaiseOrLower``. It runs in the Zigbee task. Store the new values and return. Update outputs from ``loop()``.

``report()`` sends the current temperature and running state to bound coordinators. ``reportLocalTemperature()`` sends only the room temperature. Either report is sent through the binding table.

.. code-block:: arduino

    bool report();
    bool reportLocalTemperature();

Example
-------

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_HVAC/Zigbee_HVAC.ino
    :language: arduino
