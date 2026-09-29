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
* Local ``open()`` / ``close()`` after ``Matter.begin()`` (same paths as a Matter controller).
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

* ``defaultOpenDurationSeconds`` - Reported as ``DefaultOpenDuration`` and used by ``open()`` with no argument (``0`` = none).

This function will return ``true`` if successful, ``false`` otherwise.

``open()`` / ``close()`` require ``Matter.begin()`` to have run so the live ``ValveConfigurationAndControlCluster`` exists and the delegate is attached in ``onStackStarted()``.

end
^^^

Stops processing Matter water valve events and releases the valve delegate.

.. code-block:: arduino

    void end();

Callbacks
*********

onOpen
^^^^^^

Sets a callback for when the valve is commanded open (Matter controller or local ``open()``). Return ``false`` if open could not be completed; the library keeps the valve reported as closed.

.. code-block:: arduino

    void onOpen(EndPointOpenCB onOpenCB);

The callback signature is:

.. code-block:: arduino

    bool onOpenCallback();

onClose
^^^^^^^

Sets a callback for when the valve is commanded closed (controller, local ``close()``, or end of a timed open). The callback has no return value; the CHIP delegate does not accept a close failure result.

.. code-block:: arduino

    void onClose(EndPointCloseCB onCloseCB);

The callback signature is:

.. code-block:: arduino

    void onCloseCallback();

Valve Control
*************

open
^^^^

Opens the valve indefinitely, for ``defaultOpenDurationSeconds`` (set in ``begin()``), or for an explicit duration.

.. code-block:: arduino

    bool open();
    bool open(uint32_t durationSeconds);

close
^^^^^

Closes the valve.

.. code-block:: arduino

    bool close();

setValveFault
^^^^^^^^^^^^^

Reports or clears fault bits (``ValveFault_t``). May be called before ``Matter.begin()``; the value is cached and applied when the stack starts.

.. code-block:: arduino

    bool setValveFault(uint16_t fault);

This function will return ``true`` if successful, ``false`` otherwise.

Example
-------

Water Valve
**********

.. literalinclude:: ../../../libraries/Matter/examples/Control/MatterWaterValve/MatterWaterValve.ino
    :language: arduino
