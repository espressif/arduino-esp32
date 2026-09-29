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
* Tank volume and tank percentage (optional WHM features provisioned in ``begin()``).
* Boost state.
* Water heater mode (Off / Manual / Eco).
* Automatic **HeatDemand** sync when system mode, boost, or heater types change (including Matter controller writes).
* Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
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

Creates the Water Heater device type and adds **EnergyManagement** and **TankPercent** features to Water Heater Management so ``TankVolume`` and ``TankPercentage`` attributes exist. Call before ``Matter.begin()``; calling ``begin()`` after the Matter stack has started fails.

.. code-block:: arduino

    bool begin();

This function will return ``true`` if successful, ``false`` otherwise.

Typical usage:

.. code-block:: arduino

    waterHeater.begin();
    waterHeater.setHeaterTypes(MatterWaterHeater::IMMERSION_ELEMENT_1);
    waterHeater.setTankVolume(100);
    waterHeater.setSystemMode(MatterWaterHeater::SYSTEM_MODE_HEAT);
    Matter.begin();

end
^^^

Stops processing Matter water heater events.

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

HeaterType_t
^^^^^^^^^^^^

Heater type bitmap (``HeaterTypes`` / ``HeatDemand`` use the same bit values):

* ``IMMERSION_ELEMENT_1``, ``IMMERSION_ELEMENT_2``, ``HEAT_PUMP``, ``BOILER``, ``OTHER``.

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

setHeatingSetpoint / getHeatingSetpoint
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set or read the occupied heating setpoint in degrees Celsius.

.. code-block:: arduino

    bool setHeatingSetpoint(float temperature);
    float getHeatingSetpoint();

Water Heater Management
***********************

setSystemMode / setBoostState / setWaterHeaterMode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool setSystemMode(SystemMode_t mode);
    bool setBoostState(BoostState_t state);
    bool setWaterHeaterMode(WaterHeaterMode_t mode);

``setSystemMode()`` and ``setBoostState()`` update **HeatDemand** automatically. When a Matter controller writes **SystemMode**, **BoostState**, or **HeaterTypes**, the endpoint runs the same **HeatDemand** sync logic.

setHeaterTypes / setHeatDemand
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool setHeaterTypes(uint8_t heaterTypes);
    bool setHeatDemand(uint8_t heatDemand);

Use ``setHeatDemand()`` only for finer-grained reporting while system mode stays Heat (for example, no active element once the setpoint is reached).

setTankVolume / setTankPercentage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool setTankVolume(uint16_t tankVolume);
    bool setTankPercentage(uint8_t tankPercentage);

Attributes exist after ``begin()`` provisions the optional WHM features. If attributes are missing, values are cached locally.

Example
-------

Water Heater
************

.. literalinclude:: ../../../libraries/Matter/examples/Control/MatterWaterHeater/MatterWaterHeater.ino
    :language: arduino
