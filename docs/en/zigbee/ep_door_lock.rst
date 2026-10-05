##############
ZigbeeDoorLock
##############

About
-----

The ``ZigbeeDoorLock`` class provides a Zigbee endpoint for door lock devices in a Home Automation (HA) network. This endpoint implements the Door Lock cluster server and handles the Lock Door and Unlock Door commands.

**Features:**
* Lock Door / Unlock Door command callbacks (the application can reject a command)
* LockState attribute with automatic reporting
* Configurable lock type
* Zigbee HA standard compliance

**Not supported yet:** PIN codes, user management, schedules and operation event notifications.

**Use Cases:**
* Smart door locks
* Electric strikes and gate latches

API Reference
-------------

Constructor
***********

ZigbeeDoorLock
^^^^^^^^^^^^^^

Creates a new Zigbee door lock endpoint.

.. code-block:: arduino

    ZigbeeDoorLock(uint8_t endpoint);

* ``endpoint`` - Endpoint number (1-254)

Enums
*****

ZigbeeDoorLockState
^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    enum ZigbeeDoorLockState {
      DOOR_LOCK_STATE_NOT_FULLY_LOCKED,
      DOOR_LOCK_STATE_LOCKED,
      DOOR_LOCK_STATE_UNLOCKED,
      DOOR_LOCK_STATE_UNDEFINED,  // Initial state
    };

ZigbeeDoorLockType
^^^^^^^^^^^^^^^^^^

Lock types defined by the Door Lock cluster (``DOOR_LOCK_TYPE_DEAD_BOLT`` (default), ``DOOR_LOCK_TYPE_MAGNETIC``, ``DOOR_LOCK_TYPE_OTHER``, ``DOOR_LOCK_TYPE_MORTISE``, ``DOOR_LOCK_TYPE_RIM``, ``DOOR_LOCK_TYPE_LATCH_BOLT``, ``DOOR_LOCK_TYPE_CYLINDRICAL_LOCK``, ``DOOR_LOCK_TYPE_TUBULAR_LOCK``, ``DOOR_LOCK_TYPE_INTERCONNECTED_LOCK``, ``DOOR_LOCK_TYPE_DEAD_LATCH``, ``DOOR_LOCK_TYPE_DOOR_FURNITURE``).

API Methods
***********

setLockType
^^^^^^^^^^^

Sets the lock type. Must be called before ``Zigbee.addEndpoint()``.

.. code-block:: arduino

    bool setLockType(ZigbeeDoorLockType lock_type);

setLockState
^^^^^^^^^^^^

Sets the LockState attribute. After ``Zigbee.begin()`` the new state is also reported to the bound devices.
Use this to report local operations of the lock (key, button, keypad).

.. code-block:: arduino

    bool setLockState(ZigbeeDoorLockState state);
    bool setLocked();
    bool setUnlocked();

These functions return ``true`` if successful, ``false`` otherwise.

getLockState / isLocked
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    ZigbeeDoorLockState getLockState();
    bool isLocked();

Event Handling
**************

onLock / onUnlock
^^^^^^^^^^^^^^^^^

Sets the callbacks called when the Lock Door / Unlock Door command is received.
The callback performs the physical action and returns ``true`` on success. When it returns ``false``, the command is answered
with a failure status and the LockState attribute is not changed. Without a callback the command is accepted and only the attribute is updated.

.. code-block:: arduino

    void onLock(bool (*callback)());
    void onUnlock(bool (*callback)());

The callbacks run in the Zigbee stack context, keep them short and do not call Zigbee APIs from them.

Example
-------

Door Lock Implementation
************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Door_Lock/Zigbee_Door_Lock.ino
    :language: arduino
