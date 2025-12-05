##################################
MatterTemperatureControlledCabinet
##################################

About
-----

The ``MatterTemperatureControlledCabinet`` class provides a temperature controlled cabinet endpoint for Matter networks. This endpoint implements the Matter temperature control standard for managing temperature setpoints with min/max limits, step control (always enabled), or temperature level support.

**Features:**

* Two initialization modes:

   - **Temperature Number Mode** (``begin(tempSetpoint, minTemp, maxTemp, step)``): Temperature setpoint control with min/max limits and step control
   - **Temperature Level Mode** (``begin(supportedLevels, levelCount, selectedLevel)``): Temperature level control with array of supported levels

* 1/100th degree Celsius precision (for temperature_number mode)
* Min/max temperature limits with validation (temperature_number mode)
* Temperature step control (temperature_number mode, always enabled)
* Temperature level array support (temperature_level mode)
* Automatic setpoint validation against limits
* Feature validation - methods return errors if called with wrong feature mode
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Important:** The ``temperature_number`` and ``temperature_level`` features are **mutually exclusive**. Only one can be enabled at a time. Use ``begin(tempSetpoint, minTemp, maxTemp, step)`` for temperature_number mode or ``begin(supportedLevels, levelCount, selectedLevel)`` for temperature_level mode.

**Use Cases:**

* Refrigerators and freezers
* Wine coolers
* Medical storage cabinets
* Laboratory equipment
* Food storage systems
* Temperature-controlled storage units

API Reference
-------------

Constructor
***********

MatterTemperatureControlledCabinet
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Creates a new Matter temperature controlled cabinet endpoint.

.. code-block:: arduino

    MatterTemperatureControlledCabinet();

Initialization
**************

begin
^^^^^

Initializes the Matter temperature controlled cabinet endpoint with **temperature_number** feature mode. This enables temperature setpoint control with min/max limits and optional step.

**Note:** This mode is mutually exclusive with temperature_level mode. Use ``begin(supportedLevels, levelCount, selectedLevel)`` for temperature level control.

.. code-block:: arduino

    bool begin(double tempSetpoint = 0.00, double minTemperature = -10.0, double maxTemperature = 32.0, double step = 0.50);

* ``tempSetpoint`` - Initial temperature setpoint in Celsius (default: 0.00)
* ``minTemperature`` - Minimum allowed temperature in Celsius (default: -10.0)
* ``maxTemperature`` - Maximum allowed temperature in Celsius (default: 32.0)
* ``step`` - Initial temperature step value in Celsius (default: 0.50)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** The implementation stores temperature with 1/100th degree Celsius precision internally. The temperature_step feature is always enabled for temperature_number mode, allowing ``setStep()`` to be called later even if step is not provided in ``begin()``.

begin (overloaded)
^^^^^^^^^^^^^^^^^^

Initializes the Matter temperature controlled cabinet endpoint with **temperature_level** feature mode. This enables temperature level control with an array of supported levels.

**Note:** This mode is mutually exclusive with temperature_number mode. Use ``begin(tempSetpoint, minTemp, maxTemp, step)`` for temperature setpoint control.

.. code-block:: arduino

    bool begin(uint8_t *supportedLevels, uint16_t levelCount, uint8_t selectedLevel = 0);

* ``supportedLevels`` - Pointer to array of temperature level values (uint8_t, 0-255)
* ``levelCount`` - Number of levels in the array (maximum: 16)
* ``selectedLevel`` - Initial selected temperature level (default: 0)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** The maximum number of supported levels is 16 (defined by ``temperature_control::k_max_temp_level_count``). The array is copied internally, so it does not need to remain valid after the function returns. This method uses a custom endpoint implementation that properly supports the temperature_level feature.

end
^^^

Stops processing Matter temperature controlled cabinet events.

.. code-block:: arduino

    void end();

Temperature Setpoint Control
****************************

setTemperatureSetpoint
^^^^^^^^^^^^^^^^^^^^^^^

Sets the temperature setpoint value. The setpoint will be validated against min/max limits.

**Requires:** temperature_number feature mode (use ``begin()``)

.. code-block:: arduino

    bool setTemperatureSetpoint(double temperature);

* ``temperature`` - Temperature setpoint in Celsius

This function will return ``true`` if successful, ``false`` otherwise (e.g., if temperature is out of range or wrong feature mode).

**Note:** Temperature is stored with 1/100th degree Celsius precision. The setpoint must be within the min/max temperature range. This method will return ``false`` and log an error if called when using temperature_level mode.

getTemperatureSetpoint
^^^^^^^^^^^^^^^^^^^^^^

Gets the current temperature setpoint value.

.. code-block:: arduino

    double getTemperatureSetpoint();

This function will return the temperature setpoint in Celsius with 1/100th degree precision.

Min/Max Temperature Control
****************************

setMinTemperature
^^^^^^^^^^^^^^^^^

Sets the minimum allowed temperature.

**Requires:** temperature_number feature mode (use ``begin()``)

.. code-block:: arduino

    bool setMinTemperature(double temperature);

* ``temperature`` - Minimum temperature in Celsius

This function will return ``true`` if successful, ``false`` otherwise. Will return ``false`` and log an error if called when using temperature_level mode.

getMinTemperature
^^^^^^^^^^^^^^^^^

Gets the minimum allowed temperature.

.. code-block:: arduino

    double getMinTemperature();

This function will return the minimum temperature in Celsius with 1/100th degree precision.

setMaxTemperature
^^^^^^^^^^^^^^^^^

Sets the maximum allowed temperature.

**Requires:** temperature_number feature mode (use ``begin()``)

.. code-block:: arduino

    bool setMaxTemperature(double temperature);

* ``temperature`` - Maximum temperature in Celsius

This function will return ``true`` if successful, ``false`` otherwise. Will return ``false`` and log an error if called when using temperature_level mode.

getMaxTemperature
^^^^^^^^^^^^^^^^^

Gets the maximum allowed temperature.

.. code-block:: arduino

    double getMaxTemperature();

This function will return the maximum temperature in Celsius with 1/100th degree precision.

Temperature Step Control
*************************

setStep
^^^^^^^

Sets the temperature step value.

**Requires:** temperature_number feature mode (use ``begin()``)

.. code-block:: arduino

    bool setStep(double step);

* ``step`` - Temperature step value in Celsius

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** The temperature_step feature is always enabled when using temperature_number mode, so this method can be called at any time to set or change the step value, even if step was not provided in ``begin()``. This method will return ``false`` and log an error if called when using temperature_level mode.

getStep
^^^^^^^

Gets the current temperature step value.

.. code-block:: arduino

    double getStep();

This function will return the temperature step in Celsius with 1/100th degree precision.

Temperature Level Control (Optional)
*************************************

setSelectedTemperatureLevel
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the selected temperature level.

**Requires:** temperature_level feature mode (use ``begin(supportedLevels, levelCount, selectedLevel)``)

.. code-block:: arduino

    bool setSelectedTemperatureLevel(uint8_t level);

* ``level`` - Temperature level (0-255)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Temperature level and temperature number features are mutually exclusive. This method will return ``false`` and log an error if called when using temperature_number mode.

getSelectedTemperatureLevel
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the current selected temperature level.

.. code-block:: arduino

    uint8_t getSelectedTemperatureLevel();

This function will return the selected temperature level (0-255).

setSupportedTemperatureLevels
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the supported temperature levels array.

**Requires:** temperature_level feature mode (use ``begin(supportedLevels, levelCount, selectedLevel)``)

.. code-block:: arduino

    bool setSupportedTemperatureLevels(uint8_t *levels, uint16_t count);

* ``levels`` - Pointer to array of temperature level values (array is copied internally)
* ``count`` - Number of levels in the array (maximum: 16)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** The maximum number of supported levels is 16. The array is copied internally, so it does not need to remain valid after the function returns. This method will return ``false`` and log an error if called when using temperature_number mode or if count exceeds the maximum.

getSupportedTemperatureLevelsCount
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the count of supported temperature levels.

.. code-block:: arduino

    uint16_t getSupportedTemperatureLevelsCount();

This function will return the number of supported temperature levels.

Example
-------

Temperature Controlled Cabinet
******************************

The example demonstrates the temperature_number mode with dynamic temperature updates. The temperature setpoint automatically cycles between the minimum and maximum limits every 1 second using the configured step value, allowing Matter controllers to observe real-time changes. The example also monitors and logs when the initial setpoint is reached or overpassed in each direction.

.. literalinclude:: ../../../libraries/Matter/examples/MatterTemperatureControlledCabinet/MatterTemperatureControlledCabinet.ino
    :language: arduino

Temperature Controlled Cabinet (Level Mode)
********************************************

A separate example demonstrates the temperature_level mode with dynamic level updates. The temperature level automatically cycles through all supported levels every 1 second in both directions (increasing and decreasing), allowing Matter controllers to observe real-time changes. The example also monitors and logs when the initial level is reached or overpassed in each direction.

See ``MatterTemperatureControlledCabinetLevels`` example for the temperature level mode implementation.

