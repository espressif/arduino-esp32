################
ZigbeeMultistate
################

About
-----

The ``ZigbeeMultistate`` class provides multistate input and output endpoints for Zigbee networks. This endpoint implements the Zigbee Home Automation (HA) standard for multistate signal processing and control.
Multistate Input (MI) is meant to be used for sensors that provide discrete state values, such as fan speed settings, HVAC modes, or security states to be sent to the coordinator.
Multistate Output (MO) is meant to be used for actuators that require discrete state control, such as fan controllers, HVAC systems, or security devices to be controlled by the coordinator.

.. note::

    HomeAssistant ZHA does not support multistate input and output clusters yet.

Common API
----------

Constructor
***********

ZigbeeMultistate
^^^^^^^^^^^^^^^^

Creates a new Zigbee multistate endpoint.

.. code-block:: arduino

    ZigbeeMultistate(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Multistate Application Types
****************************

The ZigbeeMultistate class supports predefined application types with specific state configurations:

.. code-block:: arduino

    // Application Type 0: Fan states (Off, On, Auto)
    #define ZB_MULTISTATE_APPLICATION_TYPE_0_INDEX         0x0000
    #define ZB_MULTISTATE_APPLICATION_TYPE_0_NUM_STATES    3
    #define ZB_MULTISTATE_APPLICATION_TYPE_0_STATE_NAMES   (const char* const[]){"Off", "On", "Auto"}

    // Application Type 1: Fan speed states (Off, Low, Medium, High)
    #define ZB_MULTISTATE_APPLICATION_TYPE_1_INDEX         0x0001
    #define ZB_MULTISTATE_APPLICATION_TYPE_1_NUM_STATES    4
    #define ZB_MULTISTATE_APPLICATION_TYPE_1_STATE_NAMES   (const char* const[]){"Off", "Low", "Medium", "High"}

    // Application Type 2: HVAC modes (Auto, Heat, Cool, Off, Emergency Heat, Fan Only, Max Heat)
    #define ZB_MULTISTATE_APPLICATION_TYPE_2_INDEX         0x0002
    #define ZB_MULTISTATE_APPLICATION_TYPE_2_NUM_STATES    7
    #define ZB_MULTISTATE_APPLICATION_TYPE_2_STATE_NAMES   (const char* const[]){"Auto", "Heat", "Cool", "Off", "Emergency Heat", "Fan Only", "Max Heat"}

    // Application Type 3: Occupancy states (Occupied, Unoccupied, Standby, Bypass)
    #define ZB_MULTISTATE_APPLICATION_TYPE_3_INDEX         0x0003
    #define ZB_MULTISTATE_APPLICATION_TYPE_3_NUM_STATES    4
    #define ZB_MULTISTATE_APPLICATION_TYPE_3_STATE_NAMES   (const char* const[]){"Occupied", "Unoccupied", "Standby", "Bypass"}

    // Application Type 4: Control states (Inactive, Active, Hold)
    #define ZB_MULTISTATE_APPLICATION_TYPE_4_INDEX         0x0004
    #define ZB_MULTISTATE_APPLICATION_TYPE_4_NUM_STATES    3
    #define ZB_MULTISTATE_APPLICATION_TYPE_4_STATE_NAMES   (const char* const[]){"Inactive", "Active", "Hold"}

    // Application Type 5: Water system states (Auto, Warm-up, Water Flush, Autocalibration, Shutdown Open, Shutdown Closed, Low Limit, Test and Balance)
    #define ZB_MULTISTATE_APPLICATION_TYPE_5_INDEX         0x0005
    #define ZB_MULTISTATE_APPLICATION_TYPE_5_NUM_STATES    8
    #define ZB_MULTISTATE_APPLICATION_TYPE_5_STATE_NAMES   (const char* const[]){"Auto", "Warm-up", "Water Flush", "Autocalibration", "Shutdown Open", "Shutdown Closed", "Low Limit", "Test and Balance"}

    // Application Type 6: HVAC system states (Off, Auto, Heat Cool, Heat Only, Cool Only, Fan Only)
    #define ZB_MULTISTATE_APPLICATION_TYPE_6_INDEX         0x0006
    #define ZB_MULTISTATE_APPLICATION_TYPE_6_NUM_STATES    6
    #define ZB_MULTISTATE_APPLICATION_TYPE_6_STATE_NAMES   (const char* const[]){"Off", "Auto", "Heat Cool", "Heat Only", "Cool Only", "Fan Only"}

    // Application Type 7: Light states (High, Normal, Low)
    #define ZB_MULTISTATE_APPLICATION_TYPE_7_INDEX         0x0007
    #define ZB_MULTISTATE_APPLICATION_TYPE_7_NUM_STATES    3
    #define ZB_MULTISTATE_APPLICATION_TYPE_7_STATE_NAMES   (const char* const[]){"High", "Normal", "Low"}

    // Application Type 8: Occupancy control states (Occupied, Unoccupied, Startup, Shutdown)
    #define ZB_MULTISTATE_APPLICATION_TYPE_8_INDEX         0x0008
    #define ZB_MULTISTATE_APPLICATION_TYPE_8_NUM_STATES    4
    #define ZB_MULTISTATE_APPLICATION_TYPE_8_STATE_NAMES   (const char* const[]){"Occupied", "Unoccupied", "Startup", "Shutdown"}

    // Application Type 9: Time states (Night, Day, Hold)
    #define ZB_MULTISTATE_APPLICATION_TYPE_9_INDEX         0x0009
    #define ZB_MULTISTATE_APPLICATION_TYPE_9_NUM_STATES    3
    #define ZB_MULTISTATE_APPLICATION_TYPE_9_STATE_NAMES   (const char* const[]){"Night", "Day", "Hold"}

    // Application Type 10: HVAC control states (Off, Cool, Heat, Auto, Emergency Heat)
    #define ZB_MULTISTATE_APPLICATION_TYPE_10_INDEX         0x000A
    #define ZB_MULTISTATE_APPLICATION_TYPE_10_NUM_STATES    5
    #define ZB_MULTISTATE_APPLICATION_TYPE_10_STATE_NAMES   (const char* const[]){"Off", "Cool", "Heat", "Auto", "Emergency Heat"}

    // Application Type 11: Water control states (Shutdown Closed, Shutdown Open, Satisfied, Mixing, Cooling, Heating, Suppl Heat)
    #define ZB_MULTISTATE_APPLICATION_TYPE_11_INDEX         0x000B
    #define ZB_MULTISTATE_APPLICATION_TYPE_11_NUM_STATES    7
    #define ZB_MULTISTATE_APPLICATION_TYPE_11_STATE_NAMES   (const char* const[]){"Shutdown Closed", "Shutdown Open", "Satisfied", "Mixing", "Cooling", "Heating", "Suppl Heat"}

    // Custom application type for user-defined states
    #define ZB_MULTISTATE_APPLICATION_TYPE_OTHER_INDEX      0xFFFF

Cluster Management
******************

addMultistateInput
^^^^^^^^^^^^^^^^^^

Adds multistate input cluster to the endpoint.

.. code-block:: arduino

    bool addMultistateInput();

This function will return ``true`` if successful, ``false`` otherwise.

addMultistateOutput
^^^^^^^^^^^^^^^^^^^

Adds multistate output cluster to the endpoint.

.. code-block:: arduino

    bool addMultistateOutput();

This function will return ``true`` if successful, ``false`` otherwise.

Multistate Input API
--------------------

Configuration Methods
*********************

setMultistateInputApplication
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the application type for the multistate input.

.. code-block:: arduino

    bool setMultistateInputApplication(uint32_t application_type);

* ``application_type`` - Application type constant (see Multistate Application Types above)

This function will return ``true`` if successful, ``false`` otherwise.

setMultistateInputDescription
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a custom description for the multistate input.

.. code-block:: arduino

    bool setMultistateInputDescription(const char *description);

* ``description`` - Description string

This function will return ``true`` if successful, ``false`` otherwise.

setMultistateInputStates
^^^^^^^^^^^^^^^^^^^^^^^^

Sets the number of states for the multistate input.

.. code-block:: arduino

    bool setMultistateInputStates(uint16_t number_of_states);

* ``number_of_states`` - Number of discrete states (1-65535)

This function will return ``true`` if successful, ``false`` otherwise.

Value Control
*************

setMultistateInput
^^^^^^^^^^^^^^^^^^

Sets the multistate input value.

.. code-block:: arduino

    bool setMultistateInput(uint16_t state);

* ``state`` - State index (0 to number_of_states-1)

This function will return ``true`` if successful, ``false`` otherwise.

getMultistateInput
^^^^^^^^^^^^^^^^^^

Gets the current multistate input value.

.. code-block:: arduino

    uint16_t getMultistateInput();

This function returns the current multistate input state.

Reporting Methods
*****************

reportMultistateInput
^^^^^^^^^^^^^^^^^^^^^

Manually reports the current multistate input value.

.. code-block:: arduino

    bool reportMultistateInput();

This function will return ``true`` if successful, ``false`` otherwise.

Multistate Output API
---------------------

Configuration Methods
*********************

setMultistateOutputApplication
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the application type for the multistate output.

.. code-block:: arduino

    bool setMultistateOutputApplication(uint32_t application_type);

* ``application_type`` - Application type constant (see Multistate Application Types above)

This function will return ``true`` if successful, ``false`` otherwise.

setMultistateOutputDescription
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets a custom description for the multistate output.

.. code-block:: arduino

    bool setMultistateOutputDescription(const char *description);

* ``description`` - Description string

This function will return ``true`` if successful, ``false`` otherwise.

setMultistateOutputStates
^^^^^^^^^^^^^^^^^^^^^^^^^

Sets the number of states for the multistate output.

.. code-block:: arduino

    bool setMultistateOutputStates(uint16_t number_of_states);

* ``number_of_states`` - Number of discrete states (1-65535)

This function will return ``true`` if successful, ``false`` otherwise.

Value Control
*************

setMultistateOutput
^^^^^^^^^^^^^^^^^^^^

Sets the multistate output value.

.. code-block:: arduino

    bool setMultistateOutput(uint16_t state);

* ``state`` - State index (0 to number_of_states-1)

This function will return ``true`` if successful, ``false`` otherwise.

getMultistateOutput
^^^^^^^^^^^^^^^^^^^

Gets the current multistate output value.

.. code-block:: arduino

    uint16_t getMultistateOutput();

This function returns the current multistate output state.

State Information
*****************

getMultistateInputStateNamesLength
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the number of state names for multistate input.

.. code-block:: arduino

    uint16_t getMultistateInputStateNamesLength();

This function returns the number of state names configured for multistate input.

getMultistateOutputStateNamesLength
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Gets the number of state names for multistate output.

.. code-block:: arduino

    uint16_t getMultistateOutputStateNamesLength();

This function returns the number of state names configured for multistate output.

Reporting Methods
*****************

reportMultistateOutput
^^^^^^^^^^^^^^^^^^^^^^^

Manually reports the current multistate output value.

.. code-block:: arduino

    bool reportMultistateOutput();

This function will return ``true`` if successful, ``false`` otherwise.

Event Handling
**************

onMultistateOutputChange
^^^^^^^^^^^^^^^^^^^^^^^^

Sets a callback function to be called when the multistate output value changes.

.. code-block:: arduino

    void onMultistateOutputChange(void (*callback)(uint16_t state));

* ``callback`` - Function to call when multistate output changes, receives the new state value

Example
-------

Multistate Input/Output
***********************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Multistate_Input_Output/Zigbee_Multistate_Input_Output.ino
    :language: arduino
