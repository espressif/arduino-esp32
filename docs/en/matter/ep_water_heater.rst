#################
MatterWaterHeater
#################

About
-----

The ``MatterWaterHeater`` class provides a water heater endpoint for Matter networks (device type ``0x050F``). This endpoint combines Water Heater Management, Water Heater Mode, and a heating-only Thermostat cluster on one endpoint.

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

Heat Demand
***********

``setSystemMode()`` and ``setBoostState()`` update **HeatDemand** automatically. Use ``setHeatDemand()`` only for finer-grained reporting (for example, no active heating once the setpoint is reached while system mode remains Heat). When a controller writes **SystemMode**, **BoostState**, or **HeaterTypes**, the endpoint runs the same **HeatDemand** sync logic.

Tank Attributes
***************

``setTankVolume()`` and ``setTankPercentage()`` use the Matter attribute store once the optional features are added in ``begin()``. If the attributes are missing, values are cached locally and a verbose log is emitted.

Example
-------

Water Heater
************

.. literalinclude:: ../../../libraries/Matter/examples/Control/MatterWaterHeater/MatterWaterHeater.ino
    :language: arduino
