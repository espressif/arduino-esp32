####################
MatterWindowCovering
####################

About
-----

The ``MatterWindowCovering`` class provides a window covering endpoint for Matter networks. This endpoint implements the Matter window covering standard for motorized blinds, shades, and other window coverings with lift and tilt control.

**Features:**
* Lift position and percentage control (0-100%)
* Tilt position and percentage control (0-100%)
* Multiple window covering types support
* Callback support for open, close, lift, tilt, and stop commands
* Integration with Apple HomeKit, Amazon Alexa, and Google Home
* Matter standard compliance

**Supported Window Covering Types:**
* ``ROLLERSHADE`` - Lift support
* ``ROLLERSHADE_2_MOTOR`` - Lift support
* ``ROLLERSHADE_EXTERIOR`` - Lift support
* ``ROLLERSHADE_EXTERIOR_2_MOTOR`` - Lift support
* ``DRAPERY`` - Lift support
* ``AWNING`` - Lift support
* ``SHUTTER`` - Tilt support
* ``BLIND_TILT_ONLY`` - Tilt support
* ``BLIND_LIFT_AND_TILT`` - Lift and Tilt support
* ``PROJECTOR_SCREEN`` - Lift support

**Use Cases:**
* Motorized blinds
* Automated shades
* Smart window coverings
* Projector screens
* Awnings and drapes

API Reference
-------------

Constructor
***********

MatterWindowCovering
^^^^^^^^^^^^^^^^^^^^

Creates a new Matter window covering endpoint.

.. code-block:: arduino

    MatterWindowCovering();

Initialization
**************

begin
^^^^^

Initializes the Matter window covering endpoint with optional initial positions and covering type.

.. code-block:: arduino

    bool begin(uint8_t liftPercent = 100, uint8_t tiltPercent = 0, WindowCoveringType_t coveringType = ROLLERSHADE);

* ``liftPercent`` - Initial lift percentage (0-100, default: 100 = fully open)
* ``tiltPercent`` - Initial tilt percentage (0-100, default: 0)
* ``coveringType`` - Window covering type (default: ROLLERSHADE). This determines which features (lift, tilt, or both) are enabled.

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Lift percentage 0 means fully closed, 100 means fully open. Tilt percentage 0 means fully closed, 100 means fully open. The covering type must be specified during initialization to ensure the correct features (lift and/or tilt) are enabled.

end
^^^

Stops processing Matter window covering events.

.. code-block:: arduino

    void end();

Lift Position Control
*********************

setLiftPosition
^^^^^^^^^^^^^^^

Sets the window covering lift position.

.. code-block:: arduino

    bool setLiftPosition(uint16_t liftPosition);

* ``liftPosition`` - Lift position value

This function will return ``true`` if successful, ``false`` otherwise.

getLiftPosition
^^^^^^^^^^^^^^^

Gets the current lift position.

.. code-block:: arduino

    uint16_t getLiftPosition();

This function will return the current lift position.

setLiftPercentage
^^^^^^^^^^^^^^^^^

Sets the window covering lift position as a percentage. This method updates the ``CurrentPositionLiftPercent100ths`` attribute, which reflects the device's actual position. The ``TargetPositionLiftPercent100ths`` attribute is set by Matter commands/apps when a new target is requested.

.. code-block:: arduino

    bool setLiftPercentage(uint8_t liftPercent);

* ``liftPercent`` - Lift percentage (0-100, where 0 is fully closed, 100 is fully open)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** When the device reaches the target position, call ``setOperationalState(LIFT, STALL)`` to indicate that movement is complete.

getLiftPercentage
^^^^^^^^^^^^^^^^^

Gets the current lift percentage.

.. code-block:: arduino

    uint8_t getLiftPercentage();

This function will return the current lift percentage (0-100).

Tilt Position Control
*********************

setTiltPosition
^^^^^^^^^^^^^^^

Sets the window covering tilt position. Note that tilt is a rotation, not a linear measurement. This method converts the absolute position to percentage using the installed limits.

.. code-block:: arduino

    bool setTiltPosition(uint16_t tiltPosition);

* ``tiltPosition`` - Tilt position value (absolute value for conversion, not a physical unit)

This function will return ``true`` if successful, ``false`` otherwise.

getTiltPosition
^^^^^^^^^^^^^^^

Gets the current tilt position. Note that tilt is a rotation, not a linear measurement.

.. code-block:: arduino

    uint16_t getTiltPosition();

This function will return the current tilt position (absolute value for conversion, not a physical unit).

setTiltPercentage
^^^^^^^^^^^^^^^^^

Sets the window covering tilt position as a percentage. This method updates the ``CurrentPositionTiltPercent100ths`` attribute, which reflects the device's actual position. The ``TargetPositionTiltPercent100ths`` attribute is set by Matter commands/apps when a new target is requested.

.. code-block:: arduino

    bool setTiltPercentage(uint8_t tiltPercent);

* ``tiltPercent`` - Tilt percentage (0-100, where 0 is fully closed, 100 is fully open)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** When the device reaches the target position, call ``setOperationalState(TILT, STALL)`` to indicate that movement is complete.

getTiltPercentage
^^^^^^^^^^^^^^^^^

Gets the current tilt percentage.

.. code-block:: arduino

    uint8_t getTiltPercentage();

This function will return the current tilt percentage (0-100).

Window Covering Type
********************

setCoveringType
^^^^^^^^^^^^^^^

Sets the window covering type.

.. code-block:: arduino

    bool setCoveringType(WindowCoveringType_t coveringType);

* ``coveringType`` - Window covering type (see Window Covering Types enum)

This function will return ``true`` if successful, ``false`` otherwise.

getCoveringType
^^^^^^^^^^^^^^^

Gets the current window covering type.

.. code-block:: arduino

    WindowCoveringType_t getCoveringType();

This function will return the current window covering type.

Installed Limit Control
***********************

setInstalledOpenLimitLift
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the installed open limit for lift (centimeters). This defines the physical position when the window covering is fully open.

.. code-block:: arduino

    bool setInstalledOpenLimitLift(uint16_t openLimit);

* ``openLimit`` - Open limit position (centimeters)

This function will return ``true`` if successful, ``false`` otherwise.

getInstalledOpenLimitLift
^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the installed open limit for lift.

.. code-block:: arduino

    uint16_t getInstalledOpenLimitLift();

This function will return the installed open limit for lift (centimeters).

setInstalledClosedLimitLift
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the installed closed limit for lift (centimeters). This defines the physical position when the window covering is fully closed.

.. code-block:: arduino

    bool setInstalledClosedLimitLift(uint16_t closedLimit);

* ``closedLimit`` - Closed limit position (centimeters)

This function will return ``true`` if successful, ``false`` otherwise.

getInstalledClosedLimitLift
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the installed closed limit for lift.

.. code-block:: arduino

    uint16_t getInstalledClosedLimitLift();

This function will return the installed closed limit for lift (centimeters).

setInstalledOpenLimitTilt
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the installed open limit for tilt (absolute value for conversion, not a physical unit). This is used for converting between absolute position and percentage.

.. code-block:: arduino

    bool setInstalledOpenLimitTilt(uint16_t openLimit);

* ``openLimit`` - Open limit absolute value

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Tilt is a rotation, not a linear measurement. These limits are used for position conversion only.

getInstalledOpenLimitTilt
^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the installed open limit for tilt.

.. code-block:: arduino

    uint16_t getInstalledOpenLimitTilt();

This function will return the installed open limit for tilt (absolute value).

setInstalledClosedLimitTilt
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the installed closed limit for tilt (absolute value for conversion, not a physical unit). This is used for converting between absolute position and percentage.

.. code-block:: arduino

    bool setInstalledClosedLimitTilt(uint16_t closedLimit);

* ``closedLimit`` - Closed limit absolute value

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Tilt is a rotation, not a linear measurement. These limits are used for position conversion only.

getInstalledClosedLimitTilt
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the installed closed limit for tilt.

.. code-block:: arduino

    uint16_t getInstalledClosedLimitTilt();

This function will return the installed closed limit for tilt (absolute value).

Target Position Control
***********************

setTargetLiftPercent100ths
^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the target lift position in percent100ths (0-10000, where 0 is fully closed, 10000 is fully open).

.. code-block:: arduino

    bool setTargetLiftPercent100ths(uint16_t liftPercent100ths);

* ``liftPercent100ths`` - Target lift position in percent100ths (0-10000)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** This sets the target position that the device should move towards. The actual position should be updated using ``setLiftPercentage()``.

getTargetLiftPercent100ths
^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the current target lift position in percent100ths.

.. code-block:: arduino

    uint16_t getTargetLiftPercent100ths();

This function will return the current target lift position in percent100ths (0-10000).

setTargetTiltPercent100ths
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the target tilt position in percent100ths (0-10000, where 0 is fully closed, 10000 is fully open).

.. code-block:: arduino

    bool setTargetTiltPercent100ths(uint16_t tiltPercent100ths);

* ``tiltPercent100ths`` - Target tilt position in percent100ths (0-10000)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** This sets the target position that the device should move towards. The actual position should be updated using ``setTiltPercentage()``.

getTargetTiltPercent100ths
^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the current target tilt position in percent100ths.

.. code-block:: arduino

    uint16_t getTargetTiltPercent100ths();

This function will return the current target tilt position in percent100ths (0-10000).

Operational Status Control
**************************

setOperationalStatus
^^^^^^^^^^^^^^^^^^^^

Sets the full operational status bitmap.

.. code-block:: arduino

    bool setOperationalStatus(uint8_t operationalStatus);

* ``operationalStatus`` - Full operational status bitmap value

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** It is recommended to use ``setOperationalState()`` to set individual field states instead of setting the full bitmap directly.

getOperationalStatus
^^^^^^^^^^^^^^^^^^^^

Gets the full operational status bitmap.

.. code-block:: arduino

    uint8_t getOperationalStatus();

This function will return the current operational status bitmap value.

setOperationalState
^^^^^^^^^^^^^^^^^^^

Sets the operational state for a specific field (LIFT or TILT). The GLOBAL field is automatically updated based on priority (LIFT > TILT).

.. code-block:: arduino

    bool setOperationalState(OperationalStatusField_t field, OperationalState_t state);

* ``field`` - Field to set (``LIFT`` or ``TILT``). ``GLOBAL`` cannot be set directly.
* ``state`` - Operational state (``STALL``, ``MOVING_UP_OR_OPEN``, or ``MOVING_DOWN_OR_CLOSE``)

This function will return ``true`` if successful, ``false`` otherwise.

**Note:** Only ``LIFT`` and ``TILT`` fields can be set directly. The ``GLOBAL`` field is automatically updated based on the active field (LIFT has priority over TILT).

getOperationalState
^^^^^^^^^^^^^^^^^^^

Gets the operational state for a specific field.

.. code-block:: arduino

    OperationalState_t getOperationalState(OperationalStatusField_t field);

* ``field`` - Field to get (``GLOBAL``, ``LIFT``, or ``TILT``)

This function will return the operational state for the specified field (``STALL``, ``MOVING_UP_OR_OPEN``, or ``MOVING_DOWN_OR_CLOSE``).

Event Handling
**************

The ``MatterWindowCovering`` class automatically detects Matter commands and calls the appropriate callbacks when registered. There are two types of callbacks:

**Target Position Callbacks** (triggered when ``TargetPosition`` attributes change):
* ``onOpen()`` - called when ``UpOrOpen`` command is received (sets target to 0% = fully open)
* ``onClose()`` - called when ``DownOrClose`` command is received (sets target to 100% = fully closed)
* ``onStop()`` - called when ``StopMotion`` command is received (sets target to current position)
* ``onGoToLiftPercentage()`` - called when ``TargetPositionLiftPercent100ths`` changes (from any command, ``setTargetLiftPercent100ths()``, or direct attribute write)
* ``onGoToTiltPercentage()`` - called when ``TargetPositionTiltPercent100ths`` changes (from any command, ``setTargetTiltPercent100ths()``, or direct attribute write)

**Current Position Callback** (triggered when ``CurrentPosition`` attributes change):
* ``onChange()`` - called when ``CurrentPositionLiftPercent100ths`` or ``CurrentPositionTiltPercent100ths`` change (after ``setLiftPercentage()``/``setTiltPercentage()`` are called or when a Matter controller updates these attributes directly)

**Important:** ``onChange()`` is **not** automatically called when Matter commands are executed. Commands modify ``TargetPosition``, not ``CurrentPosition``. To trigger ``onChange()``, your ``onGoToLiftPercentage()`` or ``onGoToTiltPercentage()`` callback must call ``setLiftPercentage()`` or ``setTiltPercentage()`` when the physical device actually moves.

**Note:** All callbacks are optional. If a specific callback is not registered, only the generic ``onGoToLiftPercentage()`` or ``onGoToTiltPercentage()`` callbacks will be called (if registered).

onOpen
^^^^^^

Sets a callback function to be called when the ``UpOrOpen`` command is received from a Matter controller. This command sets the target position to 0% (fully open).

.. code-block:: arduino

    void onOpen(EndPointOpenCB onChangeCB);

* ``onChangeCB`` - Function to call when ``UpOrOpen`` command is received

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback();

onClose
^^^^^^^

Sets a callback function to be called when the ``DownOrClose`` command is received from a Matter controller. This command sets the target position to 100% (fully closed).

.. code-block:: arduino

    void onClose(EndPointCloseCB onChangeCB);

* ``onChangeCB`` - Function to call when ``DownOrClose`` command is received

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback();

onGoToLiftPercentage
^^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when ``TargetPositionLiftPercent100ths`` changes. This is triggered by:
* Matter commands: ``UpOrOpen``, ``DownOrClose``, ``StopMotion``, ``GoToLiftPercentage``
* Calling ``setTargetLiftPercent100ths()``
* Direct attribute writes to ``TargetPositionLiftPercent100ths``

This callback is always called when the target lift position changes, regardless of which command or method was used to change it.

**Note:** This callback receives the **target** position. To update the **current** position (which triggers ``onChange()``), call ``setLiftPercentage()`` when the physical device actually moves.

.. code-block:: arduino

    void onGoToLiftPercentage(EndPointLiftCB onChangeCB);

* ``onChangeCB`` - Function to call when target lift percentage changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(uint8_t liftPercent);

* ``liftPercent`` - Target lift percentage (0-100, where 0 is fully closed, 100 is fully open)

onGoToTiltPercentage
^^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when ``TargetPositionTiltPercent100ths`` changes. This is triggered by:
* Matter commands: ``UpOrOpen``, ``DownOrClose``, ``StopMotion``, ``GoToTiltPercentage``
* Calling ``setTargetTiltPercent100ths()``
* Direct attribute writes to ``TargetPositionTiltPercent100ths``

This callback is always called when the target tilt position changes, regardless of which command or method was used to change it.

**Note:** This callback receives the **target** position. To update the **current** position (which triggers ``onChange()``), call ``setTiltPercentage()`` when the physical device actually moves.

.. code-block:: arduino

    void onGoToTiltPercentage(EndPointTiltCB onChangeCB);

* ``onChangeCB`` - Function to call when target tilt percentage changes

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(uint8_t tiltPercent);

* ``tiltPercent`` - Target tilt percentage (0-100, where 0 is fully closed, 100 is fully open)

onStop
^^^^^^

Sets a callback function to be called when the ``StopMotion`` command is received from a Matter controller. This command sets the target position to the current position, effectively stopping any movement.

.. code-block:: arduino

    void onStop(EndPointStopCB onChangeCB);

* ``onChangeCB`` - Function to call when ``StopMotion`` command is received

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback();

onChange
^^^^^^^^

Sets a callback function to be called when ``CurrentPositionLiftPercent100ths`` or ``CurrentPositionTiltPercent100ths`` attributes change. This is different from ``onGoToLiftPercentage()`` and ``onGoToTiltPercentage()``, which are called when ``TargetPosition`` attributes change.

**When ``onChange()`` is called:**
* When ``CurrentPositionLiftPercent100ths`` changes (after ``setLiftPercentage()`` is called or when a Matter controller updates this attribute directly)
* When ``CurrentPositionTiltPercent100ths`` changes (after ``setTiltPercentage()`` is called or when a Matter controller updates this attribute directly)

**Important:** ``onChange()`` is **not** automatically called when Matter commands are executed. Commands modify ``TargetPosition`` attributes, which trigger ``onGoToLiftPercentage()`` or ``onGoToTiltPercentage()`` callbacks instead. To trigger ``onChange()`` after a command, your ``onGoToLiftPercentage()`` or ``onGoToTiltPercentage()`` callback must call ``setLiftPercentage()`` or ``setTiltPercentage()`` to update the ``CurrentPosition`` attributes when the physical device actually moves.

.. code-block:: arduino

    void onChange(EndPointCB onChangeCB);

* ``onChangeCB`` - Function to call when current position attributes change

The callback signature is:

.. code-block:: arduino

    bool onChangeCallback(uint8_t liftPercent, uint8_t tiltPercent);

* ``liftPercent`` - Current lift percentage (0-100)
* ``tiltPercent`` - Current tilt percentage (0-100)

updateAccessory
^^^^^^^^^^^^^^^

Updates the state of the window covering using the current Matter internal state.

.. code-block:: arduino

    void updateAccessory();

This function will call the registered callback with the current state.

Example
-------

Window Covering
***************

.. literalinclude:: ../../../libraries/Matter/examples/MatterWindowCovering/MatterWindowCovering.ino
    :language: arduino

