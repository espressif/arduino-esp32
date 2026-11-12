#################
MatterWaterHeater
#################

About
-----

The ``MatterWaterHeater`` class provides a water heater endpoint for Matter networks (device type ``0x050F``). This endpoint combines **Water Heater Management**, **Water Heater Mode**, and a heating-only **Thermostat** cluster on one endpoint.

**Features:**

* Local temperature and occupied heating setpoint (Thermostat).
* System mode (Off / Heat).
* Heater types and heat demand bitmaps.
* Tank volume, tank percentage, and estimated heat required (optional WHM features provisioned in ``begin()``).
* Boost state, including hub **Boost** / **CancelBoost** (duration, one-shot, optional temporary setpoint).
* Water heater mode (Off / Manual / Eco).
* Automatic **HeatDemand** sync when system mode, boost, water heater mode, or heater types change. A controller **SystemMode** write (Off or Heat) and **ChangeToMode** use that same sync.
* Thermostat heating setpoint limits (``AbsMin`` / ``AbsMax`` / ``Min`` / ``Max``) so controllers can show a heating range.
* Matter standard compliance.

**Use Cases:**

* Domestic hot water tanks.
* Heat-pump water heaters.
* Boiler / immersion heater appliances.

API Reference
-------------

Constructor
***********

MatterWaterHeater
^^^^^^^^^^^^^^^^^

Creates a new Matter water heater endpoint.

.. code-block:: arduino

    MatterWaterHeater();

Initialization
**************

begin
^^^^^

Creates the Water Heater device type and installs the Water Heater Management delegate (**Boost** / **CancelBoost**).

Adds **EnergyManagement** and **TankPercent** so ``TankVolume``, ``EstimatedHeatRequired``, and ``TankPercentage`` exist. Enables the Water Heater Mode **OnOff** feature so the Off mode tag is valid. Also adds Thermostat ``AbsMin`` / ``AbsMax`` / ``Min`` / ``MaxHeatSetpointLimit``. Pins Water Heater Mode to **Manual** so the mode server does not start on **Off**.

Call ``begin()`` before ``Matter.begin()``. A call after the stack has started fails.

.. code-block:: arduino

    bool begin();

This function will return ``true`` if successful, ``false`` otherwise.

Typical usage:

.. code-block:: arduino

    waterHeater.begin();
    waterHeater.setHeaterTypes(MatterWaterHeater::IMMERSION_ELEMENT_1);
    waterHeater.setTankVolume(100); // liters
    waterHeater.setSystemMode(MatterWaterHeater::SYSTEM_MODE_HEAT);
    Matter.begin();

end
^^^

Stops sketch-side updates. If a Boost session is active, restores any temporary heating setpoint and clears Boost (no **BoostEnded** event). When the saved target is outside the current ``Min`` / ``Max``, the temporary setpoint stays and the saved target is kept until a later limit change makes it legal. The temporary value is not clamped.

Before ``Matter.begin()``, ``end()`` then destroys the endpoint, clears TagList state, and frees both delegates so ``begin()`` can be called again. After ``Matter.begin()``, ``end()`` shuts down Water Heater Mode, detaches both delegates, and clears the endpoint callback pointer. The delegates stay allocated with no owner, so destroying the Arduino object does not leave CHIP calling into freed memory. A queued setpoint restore is dropped once the delegates are detached. The Matter endpoint stays until reboot; a later ``begin()`` fails.

.. code-block:: arduino

    void end();

Modes and Types
***************

WaterHeaterMode_t
^^^^^^^^^^^^^^^^^

Water heater mode enumeration:

* ``WATER_HEATER_MODE_OFF`` - Off.
* ``WATER_HEATER_MODE_MANUAL`` - Manual.
* ``WATER_HEATER_MODE_ECO`` - Eco.

SystemMode_t
^^^^^^^^^^^^

Thermostat system mode (heating-only endpoint):

* ``SYSTEM_MODE_OFF`` - Off.
* ``SYSTEM_MODE_HEAT`` - Heat.

A controller write of any other System Mode is rejected.

HeaterType_t
^^^^^^^^^^^^

Heater type bitmap. ``HeaterTypes`` is the sources the appliance **has**. ``HeatDemand`` uses the **same bit values** for the sources **currently drawing power**:

* ``IMMERSION_ELEMENT_1`` (``0x01``), ``IMMERSION_ELEMENT_2`` (``0x02``), ``HEAT_PUMP`` (``0x04``), ``BOILER`` (``0x08``), ``OTHER`` (``0x10``).

``HeatDemand`` is not Eco / Manual / Boost / Off. Those are Water Heater Mode, Thermostat **SystemMode**, and **BoostState**.

A single-source tank reports one bit while that source is on, and ``0`` when it is not. An appliance can advertise several bits in ``HeaterTypes`` and report a **subset** in ``HeatDemand``: boiler in Manual, heat pump in Eco, boiler plus immersion in Boost.

BoostState_t
^^^^^^^^^^^^

* ``BOOST_INACTIVE`` - Boost inactive.
* ``BOOST_ACTIVE`` - Boost active.

Temperature and Setpoint
************************

setLocalTemperature / getLocalTemperature
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Report or read local water temperature in degrees Celsius.

.. code-block:: arduino

    bool setLocalTemperature(float temperature);
    float getLocalTemperature();
    bool setLocalTemperatureRaw(int16_t temperature);
    int16_t getLocalTemperatureRaw();

Raw values are hundredths of a degree Celsius (the Thermostat cluster unit).

setHeatingSetpoint / getHeatingSetpoint
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set or read the occupied heating setpoint in degrees Celsius.

.. code-block:: arduino

    bool setHeatingSetpoint(float temperature);
    float getHeatingSetpoint();
    bool setHeatingSetpointRaw(int16_t temperature);
    int16_t getHeatingSetpointRaw();

Water Heater Management
***********************

setSystemMode / setBoostState / setWaterHeaterMode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool setSystemMode(SystemMode_t mode);
    bool setBoostState(BoostState_t state);
    bool setWaterHeaterMode(WaterHeaterMode_t mode);

``setSystemMode()``, ``setBoostState()``, and ``setWaterHeaterMode()`` update **HeatDemand**. A controller **SystemMode** write does the same sync. Only Off and Heat are accepted.

**HeaterTypes**, **HeatDemand**, **TankVolume**, **TankPercentage**, **EstimatedHeatRequired**, and **BoostState** are code-driven (the WHM Delegate). Do not write them with ``updateAttributeVal()``. CHIP and the hub read the Arduino cache. The setters report changes to subscribers. **HeaterTypes** is not client-writable.

A controller **Boost** sets **BoostState** to Active. It may apply a temporary heating setpoint and arm a duration timer.

``duration`` ``0`` is a local extension. Boost stays active until **CancelBoost** or ``setBoostState(BOOST_INACTIVE)``, with no auto-timeout.

**oneShot** ends Boost once the local temperature has been reached. If a target tank percentage is present, that percentage must be reached too. A heating-setpoint write that is already reached ends the session as well, including when ``duration`` is 0.

**CancelBoost** and ``setBoostState(BOOST_INACTIVE)`` restore a temporary setpoint and emit **BoostEnded**. Once the stack is running, that completion holds the CHIP stack lock for the whole transition. If the saved setpoint is outside ``Min`` / ``Max``, Boost still ends, the temporary target stays, and the saved value is retried after a later limit change.

**EmergencyBoost** is ignored. No extra heat sources are turned on, and the command still succeeds. **TargetReheat** is rejected with ``InvalidCommand``.

Thermostat **SystemMode** Heat/Off is independent of Water Heater Mode (Off / Manual / Eco). A controller **Boost** does not change Water Heater Mode.

``setWaterHeaterMode()`` updates the CHIP mode-base **CurrentMode** (Off / Manual / Eco). Before ``Matter.begin()`` the value is cached and applied when the mode server starts.

A controller ``ChangeToMode`` command updates the same cache and **HeatDemand**. Mode Off stops normal heating demand. An active Boost still demands heat.

setHeaterTypes / setHeatDemand
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool setHeaterTypes(uint8_t heaterTypes);
    bool setHeatDemand(uint8_t heatDemand);

``HeaterTypes`` is the product capability (which heat sources exist). ``HeatDemand`` is which of those sources are drawing power **right now** (a subset of ``HeaterTypes``, or ``0``).

``setSystemMode()``, ``setBoostState()``, ``setWaterHeaterMode()``, and ``setHeaterTypes()`` call ``syncHeatDemand()``.

* **Boost** copies ``HeaterTypes`` into ``HeatDemand``, even when System Mode or Water Heater Mode is Off.
* Otherwise, **SystemMode** Heat with Water Heater Mode not Off copies ``HeaterTypes``.
* Every other case sets ``HeatDemand`` to ``0``.

``setHeatDemand()`` can report a subset while System Mode stays Heat, for example ``0`` once the tank is at the setpoint. It returns false when any bit is outside ``HeaterTypes``. The next sync replaces a legal subset with ``0`` or every ``HeaterTypes`` bit. The value is stored in the Arduino cache that ``GetHeatDemand()`` returns.

Example Serial logs print that bitmap in words, not hex. The basic sketch prints ``on`` / ``idle``.

``MatterWaterHeaterAdvanced`` advertises every ``HeaterType_t`` bit and names them: ``Immersion 1``, ``Immersion 2``, ``Heat pump``, ``Boiler``, ``Other``. Combinations use ``+``. While heating it reports a subset:

* **Manual** uses the boiler (a common market source).
* **Eco** uses the heat pump.
* **Boost** uses the boiler plus both immersion elements.
* ``idle`` when the tank is at the goal or not heating.

``OTHER`` stays in ``HeaterTypes`` so the Serial helper has a name for that bit.

setTankVolume / setTankPercentage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool setTankVolume(uint16_t tankVolume);
    bool setTankPercentage(uint8_t tankPercentage);

``TankVolume`` is the tank capacity in liters (``uint16``). ``TankPercentage`` is 0–100; values above 100 are rejected. ``begin()`` provisions the optional WHM features so the Delegate can serve these attributes. The setters update the Arduino cache (``GetTankVolume()`` / ``GetTankPercentage()``) and report; they do not use the ember store.

setEstimatedHeatRequired / getEstimatedHeatRequired
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Report or read **EstimatedHeatRequired** (EnergyManagement) in milliWatt-hours. Controllers often display this as kWh (``1 kWh = 1_000_000 mWh``).

.. code-block:: arduino

    bool setEstimatedHeatRequired(int64_t energy_mWh);
    int64_t getEstimatedHeatRequired();

This is the remaining energy to raise the tank to the heating goal, not the current heat rate and not Eco / Boost / Off themselves. The sketch computes it (for example from tank volume and ``ΔT``). Values below ``0`` are rejected. The setter updates the Arduino cache (``GetEstimatedHeatRequired()``) and reports; it does not use the ember store.

Heating Setpoint Limits
***********************

These Thermostat attributes are the **controller setpoint range** (the target dial), not local tank temperature. ``setLocalTemperature()`` can report values outside this range.

``begin()`` creates the attributes with defaults **20 °C** (``AbsMin`` / ``Min``) and **85 °C** (``AbsMax`` / ``Max``). After ``begin()``, the sketch can change them:

.. code-block:: arduino

    bool setMinimumHeatingSetpoint(float temperature);
    bool setMaximumHeatingSetpoint(float temperature);
    bool setAbsoluteMinimumHeatingSetpoint(float temperature);
    bool setAbsoluteMaximumHeatingSetpoint(float temperature);
    float getMinimumHeatingSetpoint();
    float getMaximumHeatingSetpoint();
    float getAbsoluteMinimumHeatingSetpoint();
    float getAbsoluteMaximumHeatingSetpoint();

Call the absolute setters first when narrowing the product range. ``Min`` / ``Max`` must stay inside ``AbsMin`` / ``AbsMax``.

The absolute setters reject values outside **20 °C … 85 °C**. They also reject a new absolute minimum above the current maximum or absolute maximum, or a new absolute maximum below the current minimum or absolute minimum. When narrowing a limit inside the current ``Min`` / ``Max`` span, the absolute setter raises ``Min`` or lowers ``Max`` first so ``AbsMin <= Min <= Max <= AbsMax`` always holds.

A controller **OccupiedHeatingSetpoint** outside ``Min`` / ``Max`` is rejected. Limit writes that would cross are rejected too. A controller may cache the range at commissioning. A later change may need a re-interview before the dial updates.

Example
-------

Basic Water Heater
******************

Simulated tank, System Mode Heat / Off, **Boost**, target temperature, and Serial messages for controller writes. Boost heats at twice the Heat step. See ``MatterWaterHeaterAdvanced`` for a Water Heater Mode Eco temperature cap, Boost session fields, tank percentage, estimated heat required, and per-tick attribute logging.

.. literalinclude:: ../../../libraries/Matter/examples/Control/MatterWaterHeater/MatterWaterHeater.ino
    :language: arduino

Water Heater (all features)
***************************

Eco, Boost session details (duration, one-shot, temporary setpoint), tank percentage, estimated heat required, and richer logging are in
``libraries/Matter/examples/Control/MatterWaterHeaterAdvanced``.
