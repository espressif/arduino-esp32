##############
ZigbeeDoorLock
##############

About
-----

The ``ZigbeeDoorLock`` class provides a Zigbee endpoint for door lock devices in a Home Automation (HA) network. This endpoint implements the Door Lock cluster server and handles the Lock Door, Unlock Door and PIN code user commands.

**Features:**
* Lock Door / Unlock Door command callbacks (the application can reject a command)
* LockState attribute with automatic reporting
* PIN code users (Set/Get/Clear PIN Code, Set User Status) with optional NVS storage
* Operation event notifications (keypad, manual, RF)
* Persistent LockState after a reboot
* Configurable lock type
* Zigbee HA standard compliance

**Not supported yet:** schedules, RFID users, user types and event masks.

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

restoreLockState
^^^^^^^^^^^^^^^^

Keeps the LockState after a reboot. Mark the attribute persistent before ``Zigbee.addEndpoint()`` (requires esp-zigbee-lib 2.0.5 or newer)
and read it back after ``Zigbee.begin()``. The stored state is not necessarily the real position of the lock, so check the lock before using it.

.. code-block:: arduino

    zbDoorLock.setAttributePersistent(EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID);
    // ... Zigbee.begin() ...
    ZigbeeDoorLockState state = zbDoorLock.restoreLockState();  // DOOR_LOCK_STATE_UNDEFINED if nothing was stored

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

PIN code users
**************

The Zigbee coordinator (e.g. Home Assistant with ZHA) manages the PIN code users with the SetPINCode, GetPINCode, ClearPINCode and SetUserStatus commands.
The users are kept in RAM by the endpoint (and in NVS if ``setUserStorage(true)`` is called), user IDs are zero based. The PIN code is never sent back to the network
(the SendPINOverTheAir attribute is not enabled).

.. code-block:: arduino

    bool setMaxUsers(uint16_t max_users);                 // before Zigbee.addEndpoint(), default 10
    void onUserChange(void (*callback)(uint16_t user_id)); // a user was changed by the network, use it to store the users
    bool setUserStorage(bool enable);                     // store the users in NVS and restore them now, call in setup()
    void clearUsers();                                    // remove all users (also from NVS), call it before Zigbee.factoryReset()
    ZigbeeDoorLockUserStatus getUserStatus(uint16_t user_id);
    bool getUserPin(uint16_t user_id, char *pin, size_t pin_size);
    bool setUser(uint16_t user_id, ZigbeeDoorLockUserStatus status, const char *pin);  // e.g. restore stored users
    int32_t checkPin(const char *pin);                    // user ID of the enabled user with this PIN, or -1

The PIN codes are stored as plain text in NVS, use flash and NVS encryption if the device has to protect them.
``ZigbeeDoorLockUserStatus`` is ``DOOR_LOCK_USER_AVAILABLE``, ``DOOR_LOCK_USER_ENABLED`` or ``DOOR_LOCK_USER_DISABLED``.
PIN codes have 4 to 16 characters (``ZB_DOOR_LOCK_MIN_PIN_LENGTH`` / ``ZB_DOOR_LOCK_MAX_PIN_LENGTH``), the same PIN cannot be used by two users.
The ``onUserChange`` callback runs in the Zigbee stack context, keep it short.

reportOperationEvent
^^^^^^^^^^^^^^^^^^^^

Sends an operation event notification, which ZHA / Home Assistant shows as a lock event with its source and user.

.. code-block:: arduino

    bool reportOperationEvent(ZigbeeDoorLockOperationSource source, uint8_t event_code, uint16_t user_id = 0xffff, const char *pin = nullptr);

* ``source`` - ``DOOR_LOCK_SOURCE_KEYPAD``, ``DOOR_LOCK_SOURCE_RF``, ``DOOR_LOCK_SOURCE_MANUAL``, ``DOOR_LOCK_SOURCE_RFID`` or ``DOOR_LOCK_SOURCE_INDETERMINATE``
* ``event_code`` - ``ZigbeeDoorLockEvent``, e.g. ``DOOR_LOCK_EVENT_LOCK`` / ``DOOR_LOCK_EVENT_UNLOCK`` (keypad, RF, RFID), ``DOOR_LOCK_EVENT_MANUAL_LOCK`` / ``DOOR_LOCK_EVENT_MANUAL_UNLOCK`` (manual), ``DOOR_LOCK_EVENT_LOCK_FAILURE_INVALID_PIN`` / ``DOOR_LOCK_EVENT_UNLOCK_FAILURE_INVALID_PIN`` (failed PIN attempt)
* ``user_id`` - user that operated the lock, ``0xffff`` if unknown
* ``pin`` - PIN code used (sent only if SendPINOverTheAir is enabled, which it is not by default)

Example
-------

Door Lock Implementation
************************

.. literalinclude:: ../../../libraries/Zigbee/examples/Zigbee_Door_Lock/Zigbee_Door_Lock.ino
    :language: arduino
