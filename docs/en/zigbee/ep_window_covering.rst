####################
ZigbeeWindowCovering
####################

About
-----

The ``ZigbeeWindowCovering`` class provides a window covering endpoint for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for motorized blinds, shades, and other window coverings.

**Features:**
* Position control (lift and tilt)
* Multiple window covering types support
* Configurable operation modes and limits
* Status reporting and callbacks
* Safety features and limits

**Supported Window Covering Types:**
* ROLLERSHADE - Lift support
* ROLLERSHADE_2_MOTOR - Lift support
* ROLLERSHADE_EXTERIOR - Lift support
* ROLLERSHADE_EXTERIOR_2_MOTOR - Lift support
* DRAPERY - Lift support
* AWNING - Lift support
* SHUTTER - Tilt support
* BLIND_TILT_ONLY - Tilt support
* BLIND_LIFT_AND_TILT - Lift and Tilt support
* PROJECTOR_SCREEN - Lift support

API Reference
-------------

Constructor
***********

ZigbeeWindowCovering
^^^^^^^^^^^^^^^^^^^^

Creates a new Zigbee window covering endpoint.

.. code-block:: arduino

    ZigbeeWindowCovering(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Position Control
****************

setLiftPosition
^^^^^^^^^^^^^^^

Sets the window covering lift position.

.. code-block:: arduino

    bool setLiftPosition(uint16_t lift_position);

* ``lift_position`` - Lift position

This function will return ``true`` if successful, ``false`` otherwise.

setLiftPercentage
^^^^^^^^^^^^^^^^^

Sets the window covering lift position as a percentage.

.. code-block:: arduino

    bool setLiftPercentage(uint8_t lift_percentage);

* ``lift_percentage`` - Lift percentage (0-100, where 0 is fully closed, 100 is fully open)

This function will return ``true`` if successful, ``false`` otherwise.

setTiltPosition
^^^^^^^^^^^^^^^

Sets the window covering tilt position in degrees.

.. code-block:: arduino

    bool setTiltPosition(uint16_t tilt_position);

* ``tilt_position`` - Tilt position in degrees

This function will return ``true`` if successful, ``false`` otherwise.

setTiltPercentage
^^^^^^^^^^^^^^^^^

Sets the window covering tilt position as a percentage.

.. code-block:: arduino

    bool setTiltPercentage(uint8_t tilt_percentage);

* ``tilt_percentage`` - Tilt percentage (0-100)

This function will return ``true`` if successful, ``false`` otherwise.

Configuration
*************

setCoveringType
^^^^^^^^^^^^^^^

Sets the window covering type.

.. code-block:: arduino

    bool setCoveringType(ZigbeeWindowCoveringType covering_type);

* ``covering_type`` - Window covering type (see supported types above)

This function will return ``true`` if successful, ``false`` otherwise.

setConfigStatus
^^^^^^^^^^^^^^^

Sets the window covering configuration status.

.. code-block:: arduino

    bool setConfigStatus(bool operational, bool online, bool commands_reversed, bool lift_closed_loop, bool tilt_closed_loop, bool lift_encoder_controlled, bool tilt_encoder_controlled);

* ``operational`` - Operational status
* ``online`` - Online status
* ``commands_reversed`` - Commands reversed flag
* ``lift_closed_loop`` - Lift closed loop flag
* ``tilt_closed_loop`` - Tilt closed loop flag
* ``lift_encoder_controlled`` - Lift encoder controlled flag
* ``tilt_encoder_controlled`` - Tilt encoder controlled flag

This function will return ``true`` if successful, ``false`` otherwise.

setMode
^^^^^^^

Sets the window covering operation mode.

.. code-block:: arduino

    bool setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on);

* ``motor_reversed`` - Motor reversed flag
* ``calibration_mode`` - Calibration mode flag
* ``maintenance_mode`` - Maintenance mode flag
* ``leds_on`` - LEDs on flag

This function will return ``true`` if successful, ``false`` otherwise.

setLimits
^^^^^^^^^

Sets the motion limits for the window covering.

.. code-block:: arduino

    bool setLimits(uint16_t installed_open_limit_lift, uint16_t installed_closed_limit_lift, uint16_t installed_open_limit_tilt, uint16_t installed_closed_limit_tilt);

* ``installed_open_limit_lift`` - Installed open limit for lift
* ``installed_closed_limit_lift`` - Installed closed limit for lift
* ``installed_open_limit_tilt`` - Installed open limit for tilt
* ``installed_closed_limit_tilt`` - Installed closed limit for tilt

This function will return ``true`` if successful, ``false`` otherwise.

Event Handling
**************

onOpen
^^^^^^

Sets a callback function to be called when the window covering opens.

.. code-block:: arduino

    void onOpen(void (*callback)());

* ``callback`` - Function to call when window covering opens

onClose
^^^^^^^

Sets a callback function to be called when the window covering closes.

.. code-block:: arduino

    void onClose(void (*callback)());

* ``callback`` - Function to call when window covering closes

onGoToLiftPercentage
^^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when lift percentage changes.

.. code-block:: arduino

    void onGoToLiftPercentage(void (*callback)(uint8_t));

* ``callback`` - Function to call when lift percentage changes

onGoToTiltPercentage
^^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when tilt percentage changes.

.. code-block:: arduino

    void onGoToTiltPercentage(void (*callback)(uint8_t));

* ``callback`` - Function to call when tilt percentage changes

onStop
^^^^^^

Sets a callback function to be called when window covering stops.

.. code-block:: arduino

    void onStop(void (*callback)());

* ``callback`` - Function to call when window covering stops

Example
-------

Window Covering Implementation
******************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Window_Covering/Zigbee_Window_Covering.ino
    :language: arduino
