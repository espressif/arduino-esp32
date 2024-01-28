###########
Preferences
###########

About
-----

The Preferences library is unique to arduino-esp32. It should be considered as the replacement for the Arduino EEPROM library.

It uses a portion of the on-board non-volatile memory (NVS) of the ESP32 to store data. This data is retained across restarts and loss of power events to the system.

Preferences works best for storing many small values, rather than a few large values. If large amounts of data are to be stored, consider using a file system library such as LitteFS.

The Preferences library is usable by all ESP32 variants.


Header File
-----------

.. code-block:: arduino

    #include <Preferences.h>
..


Overview
--------

Library methods are provided to:
   - create a namespace;
   - open and close a namespace;
   - store and retrieve data within a namespace for supported data types;
   - determine if a key value has been initialized;
   - delete a ``key-value`` pair;
   - delete all ``key-value`` pairs in a namespace;
   - determine data types stored against a key;
   - determine the number of key entries in the namespace.

Preferences directly supports the following data types:

.. table:: **Table 1 — Preferences Data Types**
   :align: center

   +-------------------+-------------------+---------------+
   | Preferences Type  | Data Type         | Size (bytes)  |
   +===================+===================+===============+
   | Bool              | bool              | 1             |
   +-------------------+-------------------+---------------+
   | Char              | int8_t            | 1             |
   +-------------------+-------------------+---------------+
   | UChar             | uint8_t           | 1             |
   +-------------------+-------------------+---------------+
   | Short             | int16_t           | 2             |
   +-------------------+-------------------+---------------+
   | UShort            | uint16_t          | 2             |
   +-------------------+-------------------+---------------+
   | Int               | int32_t           | 4             |
   +-------------------+-------------------+---------------+
   | UInt              | uint32_t          | 4             |
   +-------------------+-------------------+---------------+
   | Long              | int32_t           | 4             |
   +-------------------+-------------------+---------------+
   | ULong             | uint32_t          | 4             |
   +-------------------+-------------------+---------------+
   | Long64            | int64_t           | 8             |
   +-------------------+-------------------+---------------+
   | ULong64           | uint64_t          | 8             |
   +-------------------+-------------------+---------------+
   | Float             | float_t           | 8             |
   +-------------------+-------------------+---------------+
   | Double            | double_t          | 8             |
   +-------------------+-------------------+---------------+
   |                   | const char*       | variable      |
   | String            +-------------------+               |
   |                   | String            |               |
   +-------------------+-------------------+---------------+
   | Bytes             | uint8_t           | variable      |
   +-------------------+-------------------+---------------+

String values can be stored and retrieved either as an Arduino String or as a null terminated ``char`` array (c-string).

Bytes type is used for storing and retrieving an arbitrary number of bytes in a namespace.


Arduino-esp32 Preferences API
-----------------------------

``begin``
**********

   Open non-volatile storage with a given namespace name from an NVS partition.

   .. code-block:: arduino

     bool begin(const char * name, bool readOnly=false, const char* partition_label=NULL)
   ..

   **Parameters**
      * ``name`` (Required)
         - Namespace name. Maximum length is 15 characters.

      * ``readOnly`` (Optional)
         - ``false`` will open the namespace in read-write mode.
         - ``true``  will open the namespace in read-only mode.
         - if omitted, the namespace is opened in read-write mode.

      * ``partition_label`` (Optional)
         - name of the NVS partition in which to open the namespace.
         - if omitted, the namespace is opened in the "``nvs``" partition.

   **Returns**
      * ``true`` if the namespace was opened successfully; ``false`` otherwise.

   **Notes**
      * If the namespace does not exist within the partition, it is first created.
      * Attempting to write a key value to a namespace open in read-only mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``end``
*********

   Close the currently opened namespace.

   .. code-block:: arduino

      void end()
   ..

   **Parameters**
      * None

   **Returns**
      * Nothing

   **Note**
      * After closing a namespace, methods used to access it will fail.


``clear``
**********

   Delete all keys and values from the currently opened namespace.

   .. code-block:: arduino

      bool clear()
   ..

   **Parameters**
      * None

   **Returns**
      * ``true`` if all keys and values were deleted; ``false`` otherwise.

   **Note**
      * the namespace name still exists afterward.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``remove``
*************

   Delete a key-value pair from the currently open namespace.

   .. code-block:: arduino

       bool remove(const char * key)
   ..

   **Parameters**
      * ``key`` (Required)
         -  the name of the key to be deleted.

   **Returns**
      * ``true`` if key-value pair was deleted; ``false`` otherwise.

   **Note**
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``isKey``
*************

   Check if a key-value pair from the currently open namespace exists.

   .. code-block:: arduino

       bool isKey(const char * key)
   ..

   **Parameters**
      * ``key`` (Required)
         -  the name of the key to be checked.

   **Returns**
      * ``true`` if key-value pair exists; ``false`` otherwise.

   **Note**
      * Attempting to check a key without a namespace being open will return false.


``putChar, putUChar``
**********************

   Store a value against a given key in the currently open namespace.

   .. code-block:: arduino

       size_t putChar(const char* key, int8_t value)
       size_t putUChar(const char* key, uint8_t value)

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - must match the data type of the method.

   **Returns**
      * ``1`` (the number of bytes stored for these data types) if the call is successful; ``0`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``putShort, putUShort``
************************

   Store a value against a given key in the currently open namespace.

   .. code-block:: arduino

       size_t putShort(const char* key, int16_t value)
       size_t putUShort(const char* key, uint16_t value)

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - must match the data type of the method.

   **Returns**
      * ``2`` (the number of bytes stored for these data types) if the call is successful; ``0`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.



``putInt, putUInt``
********************
``putLong, putULong``
**********************

   Store a value against a given key in the currently open namespace.

   .. code-block:: arduino

       size_t putInt(const char* key, int32_t value)
       size_t putUInt(const char* key, uint32_t value)
       size_t putLong(const char* key, int32_t value)
       size_t putULong(const char* key, uint32_t value)

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - must match the data type of the method.

   **Returns**
      * ``4`` (the number of bytes stored for these data types) if the call is successful; ``0`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``putLong64, putULong64``
*************************
``putFloat, putDouble``
***********************

   Store a value against a given key in the currently open namespace.

   .. code-block:: arduino

       size_t putLong64(const char* key, int64_t value)
       size_t putULong64(const char* key, uint64_t value)
       size_t putFloat(const char* key, float_t value)
       size_t putDouble(const char* key, double_t value)

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - must match the data type of the method.

   **Returns**
      * ``8`` (the number of bytes stored for these data types) if the call is successful; ``0`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``putBool``
***********

   Store a value against a given key in the currently open namespace.

   .. code-block:: arduino

          size_t putBool(const char* key, bool value)

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - must match the data type of the method.

   **Returns**
      * ``true`` if successful; ``false`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``putString``
**************

   Store a variable length value against a given key in the currently open namespace.

   .. code-block:: arduino

       size_t putString(const char* key, const char* value);
       size_t putString(const char* key, String value);

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - if ``const char*``, a null-terminated (c-string) character array.
         - if ``String``, a valid Arduino String type.

   **Returns**
      * if successful: the number of bytes stored; ``0`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``putBytes``
************

   Store a variable number of bytes against a given key in the currently open namespace.

   .. code-block:: arduino

       size_t putBytes(const char* key, const void* value, size_t len);

   ..

   **Parameters**
      * ``key`` (Required)
         - if the key does not exist in the currently opened namespace it is first created.

      * ``value`` (Required)
         - pointer to an array or buffer containing the bytes to be stored.

      * ``len`` (Required)
         - the number of bytes from ``value`` to be stored.

   **Returns**
      *  if successful: the number of bytes stored; ``0`` otherwise.

   **Notes**
      * Attempting to store a value without a namespace being open in read-write mode will fail.
      * This method operates on the bytes used by the underlying data type, not the number of elements of a given data type. The data type of ``value`` is not retained by the Preferences library afterward.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``getChar, getUChar``
*********************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      int8_t getChar(const char* key, int8_t defaultValue = 0)
      uint8_t getUChar(const char* key, uint8_t defaultValue = 0)

   ..

   **Parameters**
      * ``key`` (Required)

      * ``defaultValue`` (Optional)
         - must match the data type of the method if provided.

   **Returns**
      * the value stored against ``key`` if the call is successful.
      * ``defaultValue``, if it is provided; ``0`` otherwise.

   **Notes**
      * Attempting to retrieve a key without a namespace being available will fail.
      * Attempting to retrieve value from a non existent key will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``getShort, getUShort``
****************************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      int16_t getShort(const char* key, int16_t defaultValue = 0)
      uint16_t getUShort(const char* key, uint16_t defaultValue = 0)
   ..

   Except for the data type returned, behaves exactly like ``getChar``.



``getInt, getUInt``
*******************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

    int32_t getInt(const char* key, int32_t defaultValue = 0)
    uint32_t getUInt(const char* key, uint32_t defaultValue = 0)

   ..

   Except for the data type returned, behaves exactly like ``getChar``.


``getLong, getULong``
*********************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      int32_t getLong(const char* key, int32_t defaultValue = 0)
      uint32_t getULong(const char* key, uint32_t defaultValue = 0)

   ..

   Except for the data type returned, behaves exactly like ``getChar``.


``getLong64, getULong64``
*************************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      int64_t getLong64(const char* key, int64_t defaultValue = 0)
      uint64_t getULong64(const char* key, uint64_t defaultValue = 0)

   ..

   Except for the data type returned, behaves exactly like ``getChar``.


``getFloat``
*************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      float_t getFloat(const char* key, float_t defaultValue = NAN)

   ..

   Except for the data type returned and the value of ``defaultValue``, behaves exactly like ``getChar``.


``getDouble``
*************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      double_t getDouble(const char* key, double_t defaultValue = NAN)

   ..

   Except for the data type returned and the value of ``defaultValue``, behaves exactly like ``getChar``.


``getBool``
************

   Retrieve a value stored against a given key in the currently open namespace.

   .. code-block:: arduino

      uint8_t getUChar(const char* key, uint8_t defaultValue = 0);

   ..

   Except for the data type returned, behaves exactly like ``getChar``.


``getString``
*************

   Copy a string of ``char`` stored against a given key in the currently open namespace to a buffer.

.. code-block:: arduino

    size_t getString(const char* key, char* value, size_t len);
..

   **Parameters**
      * ``key`` (Required)
      * ``value`` (Required)
         - a buffer of a size large enough to hold ``len`` bytes
      * ``len`` (Required)
         - the number of type ``char``` to be written to the buffer pointed to by ``value``

   **Returns**
      * if successful; the number of bytes equal to ``len`` is written to the buffer pointed to by ``value``, and the method returns ``1``.
      * if the method fails, nothing is written to the buffer pointed to by ``value`` and the method returns ``0``.

   **Notes**
      * ``len`` must equal the number of bytes stored against the key or the call will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``getString``
*************

   Retrieve an Arduino String value stored against a given key in the currently open namespace.

.. code-block:: arduino

    String getString(const char* key, String defaultValue = String());

..

   **Parameters**
      * ``key`` (Required)
      * ``defaultValue`` (Optional)

   **Returns**
      * the value stored against ``key`` if the call if successful
      * if the method fails: it returns ``defaultValue``, if provided; ``""`` (an empty String) otherwise.

   **Notes**
      * ``defaultValue`` must be of type ``String``.


``getBytes``
*************

Copy a series of bytes stored against a given key in the currently open namespace to a buffer.

.. code-block:: arduino

   size_t getBytes(const char* key, void * buf, size_t len);

..

   **Parameters**
      * ``key`` (Required)
      * ``buf`` (Required)
         - a buffer of a size large enough to hold ``len`` bytes.
      * ``len`` (Required)
         - the number of bytes to be written to the buffer pointed to by ``buf``

   **Returns**
      * if successful, the number of bytes equal to ``len`` is written to buffer ``buf``, and the method returns ``len``.
      * if the method fails, nothing is written to the buffer and the method returns ``0``.

   **Notes**
      * ``len`` must equal the number of bytes stored against the key or the call will fail.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``getBytesLength``
******************

Get the number of bytes stored in the value against a key of type ``Bytes`` in the currently open namespace.

.. code-block:: arduino

   size_t getBytesLength(const char* key)

..

   **Parameters**
      * ``key`` (Required)

   **Returns**
      * if successful: the number of bytes in the value stored against ``key``; ``0`` otherwise.

   **Notes**
      * This method will fail if ``key`` is not of type ``Bytes``.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


``getType``
***********

Get the Preferences data type of a given key within the currently open namespace.

.. code-block:: arduino

   PreferenceType getType(const char* key)

..

   **Parameters**
      * ``key`` (Required)

   **Returns**
      * an ``int`` value as per Table 2 below.
      * a value of ``10`` (PT_INVALID) if the call fails.

   **Notes**
      * The return values are enumerated in ``Preferences.h``. Table 2 includes the enumerated values for information.
      * A return value can map to more than one Prefs Type.
      * The method will fail if: the namespace is not open; the key does not exist; the provided key exceeds 15 characters.

.. table:: **Table 2 — getType Return Values**
   :align: center

   +---------------+---------------+-------------------+-----------------------+
   | Return value  | Prefs Type    | Data Type         | Enumerated Value      |
   +===============+===============+===================+=======================+
   | 0             | Char          | int8_t            | PT_I8                 |
   +---------------+---------------+-------------------+-----------------------+
   | 1             | UChar         | uint8_t           | PT_U8                 |
   |               +---------------+-------------------+                       |
   |               | Bool          | bool              |                       |
   +---------------+---------------+-------------------+-----------------------+
   | 2             | Short         | int16_t           | PT_I16                |
   +---------------+---------------+-------------------+-----------------------+
   | 3             | UShort        | uint16_t          | PT_U16                |
   +---------------+---------------+-------------------+-----------------------+
   | 4             | Int           | int32_t           | PT_I32                |
   |               +---------------+                   |                       |
   |               | Long          |                   |                       |
   +---------------+---------------+-------------------+-----------------------+
   | 5             | UInt          | uint32_t          | PT_U32                |
   |               +---------------+                   |                       |
   |               | ULong         |                   |                       |
   +---------------+---------------+-------------------+-----------------------+
   | 6             | Long64        | int64_t           | PT_I64                |
   +---------------+---------------+-------------------+-----------------------+
   | 7             | ULong64       | uint64_t          | PT_U64                |
   +---------------+---------------+-------------------+-----------------------+
   | 8             | String        | String            | PT_STR                |
   |               |               +-------------------+                       |
   |               |               | \*char            |                       |
   +---------------+---------------+-------------------+-----------------------+
   | 9             | Double        | double_t          | PT_BLOB               |
   |               +---------------+-------------------+                       |
   |               | Float         | float_t           |                       |
   |               +---------------+-------------------+                       |
   |               | Bytes         | uint8_t           |                       |
   +---------------+---------------+-------------------+-----------------------+
   | 10            | \-            | \-                | PT_INVALID            |
   +---------------+---------------+-------------------+-----------------------+


``freeEntries``
***************

Get the number of free entries available in the key table of the currently open namespace.

.. code-block:: arduino

   size_t freeEntries()

..

   **Parameters**
      * none

   **Returns**
      * if successful: the number of free entries available in the key table of the currently open namespace; ``0`` otherwise.

   **Notes**
      * keys storing values of type ``Bool``, ``Char``, ``UChar``, ``Short``, ``UShort``, ``Int``, ``UInt``, ``Long``, ``ULong``, ``Long64``, ``ULong64`` use one entry in the key table.
      * keys storing values of type ``Float`` and ``Double`` use three entries in the key table.
      * Arduino or c-string ``String`` types use a minimum of two key table entries with the number of entries increasing with the length of the string.
      * keys storing values of type ``Bytes`` use a minimum of three key table entries with the number of entries increasing with the number of bytes stored.
      * A message providing the reason for a failed call is sent to the arduino-esp32 ``log_e`` facility.


..    --- EOF ----
