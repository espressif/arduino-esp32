################
MatterWaterValve
################

About
-----

The ``MatterWaterValve`` class provides a water valve endpoint for Matter networks (device type ``0x0042``). This endpoint implements the Matter Valve Configuration and Control cluster as a code-driven CHIP server object.

**Features:**

* Open indefinitely or for a timed duration (automatic close when the countdown elapses).
* ``DefaultOpenDuration`` configured in ``begin()``.
* ``ValveFault`` reporting.
* ``onOpen()`` / ``onClose()`` user callbacks bridged through a cluster ``Delegate``.
* Failed ``onOpen()`` is rolled back after CHIP ``OpenValve()`` returns so the remaining-duration timer does not keep the valve Open.
* Local ``open()`` / ``close()`` after ``Matter.begin()`` (same paths as a Matter controller).
* Open/Close only (no Level/percent control; Open commands that carry a level are ignored).
* Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
* Matter standard compliance.

**Use Cases:**

* Irrigation valves.
* Main water shutoff valves.
* Appliance supply valves.

API Reference
-------------

Constructor
***********

MatterWaterValve
^^^^^^^^^^^^^^^^

Creates a new Matter water valve endpoint.

.. code-block:: arduino

    MatterWaterValve();

Initialization
**************

begin
^^^^^

Creates the endpoint and allocates the valve delegate. Call before ``Matter.begin()``.

.. code-block:: arduino

    bool begin(uint32_t defaultOpenDurationSeconds = 0);

* ``defaultOpenDurationSeconds`` - Reported as ``DefaultOpenDuration`` when non-zero; used by ``open()`` with no argument. ``0`` omits ``DefaultOpenDuration`` and ``open()`` (no argument) opens indefinitely.

This function will return ``true`` if successful, ``false`` otherwise.

Typical usage:

.. code-block:: arduino

    WaterValve.begin();
    WaterValve.onOpen(onValveOpen);
    WaterValve.onClose(onValveClose);
    Matter.begin();

``open()`` / ``close()`` require ``Matter.begin()`` to have run so the live ``ValveConfigurationAndControlCluster`` exists and the delegate is attached in ``onStackStarted()``.

end
^^^

Stops processing Matter water valve events and releases the valve delegate.

.. code-block:: arduino

    void end();

Valve States and Faults
***********************

ValveState_t
^^^^^^^^^^^^

Valve state enumeration (``ValveStateEnum``):

* ``VALVE_STATE_CLOSED`` - Valve is closed.
* ``VALVE_STATE_OPEN`` - Valve is open.
* ``VALVE_STATE_TRANSITIONING`` - Matter spec value; this library reports ``CLOSED`` or ``OPEN`` via the delegate (transitioning is not set locally).

ValveFault_t
^^^^^^^^^^^^

Valve fault bitmap (combine with bitwise OR):

* ``VALVE_FAULT_GENERAL_FAULT``
* ``VALVE_FAULT_BLOCKED``
* ``VALVE_FAULT_LEAKING``
* ``VALVE_FAULT_NOT_CONNECTED``
* ``VALVE_FAULT_SHORT_CIRCUIT``
* ``VALVE_FAULT_CURRENT_EXCEEDED``

Callbacks
*********

onOpen
^^^^^^

Sets a callback for when the valve is commanded open (Matter controller or local ``open()``). Perform the physical open here; return ``false`` if it could not be completed. If no callback is registered, Matter state still moves to Open. The callback runs with the CHIP stack lock held—keep work short. Return ``false`` keeps the valve reported as closed. A failed Open is rolled back after CHIP ``OpenValve()`` returns (the remaining-duration timer is started after the delegate callback), which may invoke ``onClose()`` to align the cluster. Local ``open()`` does not CloseValve a second time when that rollback is already scheduled.

.. code-block:: arduino

    void onOpen(EndPointOpenCB onOpenCB);

The callback signature is:

.. code-block:: arduino

    bool onOpenCallback();

onClose
^^^^^^^

Sets a callback for when the valve is commanded closed (controller, local ``close()``, end of a timed open, or rollback after a failed ``onOpen()``). The callback has no return value; the CHIP delegate does not accept a close failure result.

.. code-block:: arduino

    void onClose(EndPointCloseCB onCloseCB);

The callback signature is:

.. code-block:: arduino

    void onCloseCallback();

Valve Control
*************

open
^^^^

``open()`` opens using ``defaultOpenDurationSeconds`` from ``begin()`` (``0`` = open indefinitely). ``open(durationSeconds)`` opens for that many seconds; pass ``0`` to open indefinitely regardless of the default.

.. code-block:: arduino

    bool open();
    bool open(uint32_t durationSeconds);

Requires ``Matter.begin()`` so the live cluster exists. This function will return ``true`` if successful, ``false`` otherwise (including when ``onOpen()`` returns ``false``).

close
^^^^^

Closes the valve. Requires ``Matter.begin()`` so the live cluster exists (same as ``open()``).

.. code-block:: arduino

    bool close();

This function will return ``true`` if successful, ``false`` otherwise.

setValveFault
^^^^^^^^^^^^^

Reports or clears fault bits (``ValveFault_t``). Pass ``0`` to clear. Call after ``WaterValve.begin()``; may be called before ``Matter.begin()`` (cached and applied in ``onStackStarted()``). While any fault bit is set, the cluster rejects Open commands (Matter controller and local ``open()``).

.. code-block:: arduino

    bool setValveFault(uint16_t fault);

This function will return ``true`` if successful, ``false`` otherwise.

State Query
***********

getCurrentState / isOpen
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    ValveState_t getCurrentState();
    ValveState_t getTargetState();
    bool isOpen();

getRemainingDuration
^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    uint32_t getOpenDuration();
    uint32_t getDefaultOpenDuration();
    uint32_t getRemainingDuration();
    uint16_t getValveFault();

Cached state is updated by the delegate (open/close paths and duration ticks).

Example
-------

Water Valve
***********

.. literalinclude:: ../../../libraries/Matter/examples/Control/MatterWaterValve/MatterWaterValve.ino
    :language: arduino
