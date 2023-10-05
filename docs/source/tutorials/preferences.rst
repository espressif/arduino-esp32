###########
Preferences
###########


Introduction
------------

The Preferences library is unique to arduino-esp32. It should be considered as the replacement for the Arduino EEPROM library.

It uses a portion of the on-board non-volatile memory (NVS) of the ESP32 to store data. This data is retained across restarts and loss of power events to the system. 

Preferences works best for storing many small values, rather than a few large values. If you need to store large amounts of data, consider using a file system library such as LitteFS.

The Preferences library is usable by all ESP32 variants.


Preferences Attributes
----------------------

Preferences data is stored in NVS in sections called a "``namespace``". Within each namespace are a set of ``key-value`` pairs. The "``key``" is the name of the data item and the "``value``" is, well, the value of that piece of data. Kind of like variables. The key is the name of the variable and the value is its value. Like variables, a ``key-value`` pair has a data type.

Multiple namespaces are permitted within NVS. The name of each namespace must be unique. The keys within that namespace are unique to that namespace. Meaning the same key name can be used in multiple namespaces without conflict.

Namespace and key names are case sensitive.

Each key name must be unique within a namespace.

Namespace and key names are character strings and are limited to a maximum of 15 characters.

Only one namespace can be open (in use) at a time.


Library Overview
----------------

Library methods are provided to:
   - create a namespace;
   - open and close a namespace;
   - store and retrieve data within a namespace for supported data types;
   - determine if a key value has been initialized;
   - delete a ``key-value`` pair;
   - delete all ``key-value`` pairs in a namespace;
   - determine data types stored against a key;
   - determine the number of key entries available in the namespace.

Preferences directly suports the following data types:

.. table:: **Table 1 — Preferences Types**
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
   |                   | const char*       |               |
   | String            +-------------------+ variable      |
   |                   | String            |               |
   +-------------------+-------------------+---------------+
   | Bytes             | uint8_t           | variable      |
   +-------------------+-------------------+---------------+

String values can be stored and retrieved either as an Arduino String or as a null terminated ``char`` array (C-string).

Bytes type is used for storing and retrieving an arbitrary number of bytes in a namespace.


Workflow
--------

Preferences workflow, once everything is initialized, is pretty simple.

To store a value:
   -  Open the namespace in read-write mode.
   -  Put the value into the key.
   -  Close the namespace.

To retrieve a value:
   -  Open the namespace in read-only mode.
   -  Use the key to get the value.
   -  Close the namespace.

*(Technically, you can retrieve a value if the namespace is open in either read-only or read-write mode but it's good practice to open the namespace in read-only mode if you are only retrieving values.)*

When storing information, a "``put[PreferencesType]``" method referenced to its key is used. 

When retrieving information a "``get[PreferencesType]``" method referenced to its key is used.

Ensuring that the data types of your “``get``'s” and “``put``'s” all match, you’re good to go.

The nuance is in initializing everything at the start.

Before you can store or retrieve anything using Preferences, both the namespace and the key within that namespace need to exist. So the workflow is:

#. Create or open the namespace.
#. Test for the existence of a key that should exist if the namespace has been initialized.
#. If that key does not exist, create the key(s).
#. Carry on with the rest of your sketch where data can now be stored and retrieved from the namespace.

Each step is discussed below.

.. note::

   From here on when referring in general to a method used to store or retrieve data we'll use the shorthand "``putX``" and "``getX``" where the "``X``" is understood to be a Preferences Type; Bool, UInt, Char, and so on from the Preferences Types table above.

..

 
Create or Open the Namespace
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In your sketch, first insert a declaration of a ``Preferences`` object by including a line like;

.. code-block:: arduino

   Preferences mySketchPrefs;    // "mySketchPrefs" is the name of the Preferences object.
                                 //  Can be whatever you want.
   
This object is used with the Preferences methods to access the namespace and the key-value pairs it contains.

A namespace is made available for use with the ``.begin`` method:

.. code-block:: arduino

  mySketchPrefs.begin("myPrefs", false)

If the namespace does not yet exist, this will create and then open the namespace ``myPrefs``.

If the namespace already exists, this will open the namespace ``myPrefs``.

If the second argument is ``false`` the namespace is opened in read-write (RW) mode — values can be stored in to and retrieved from the namespace. If it is ``true`` the namespace is opened in read-only (RO) mode — values can be retrieved from the namespace but nothing can be stored.


Test for Initial Existence of Your Key(s)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When the ESP32 boots, there is no inherent way to know if this is the very first time it has ever powered on or if it is a subsequent launch and it has run its sketch before. We can use Preferences to store information that is retained across reboots that we can read, and based on that, decide if this is a first-time run and take the required actions if so.

We do this by testing for the existence of a certain key within a namespace. If that key exists, it is safe to assume the key was created during the first-time run of the sketch and so the namespace has already been initialized.

To determine if a key exists, use:

.. code-block:: arduino

   isKey("myTestKey")

This returns ``true`` if  ``"myTestKey"`` exists in the namespace, and ``false`` if it does not.

By example, consider this code segment:

.. code-block:: arduino

   Preferences mySketchPrefs;
   String doesExist;

   mySketchPrefs.begin("myPrefs", false);   // open (or create and then open if it does not 
                                            //  yet exist) the namespace "myPrefs" in RW mode.
   
   bool doesExist = mySketchPrefs.isKey("myTestKey");
   
   if (doesExist == false) {
       /* 
          If doesExist is false, we will need to create our
           namespace key(s) and store a value into them.
      */    
       
      // Insert your "first time run" code to create your keys & assign their values below here.
   }
   else {
      /* 
          If doesExist is true, the key(s) we need have been created before
           and so we can access their values as needed during startup.
      */
      
      // Insert your "we've been here before" startup code below here.
   }



Creating Namespace Keys and Storing Values
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To create a key, we use one of the  ``.putX`` methods, matching ``"X"`` to the Preferences Type of the data we wish to store:

.. code-block:: arduino

   myPreferences.putX("myKeyName", value)

If ``"myKeyName"`` does not exist in the namespace, it is first created and then ``value`` is stored against that keyname. The namespace must be open in RW mode to do this. Note that ``value`` is not optional and must be provided with every "``.putX``" statement. Thus every key within a namespace will always hold a valid value.

An example is:

.. code-block:: arduino

   myPreferences.putFloat("pi", 3.14159265359);    // stores an float_t data type 
                                                   //  against the key "pi".

Reading Values From a Namespace
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once a key exists in a namespace and the namespace is open, its value is retrieved using one of the ``getX`` methods, matching ``"X"`` to the type of data stored against that key.

.. code-block:: arduino

   myPreferences.getX("myKeyName")

Like so:

.. code-block:: arduino

   float myFloat = myPreferences.getFloat("pi");

This will retrieve the float value from the namespace key ``"pi"`` and assign it to the float type variable ``myFloat``.


Summary
~~~~~~~

So the basics of using Preferences are:

   #. You cannot store into or retrieve from a ``key-value`` pair until a namespace is created and opened and the key exists in that namespace.
   
   #. If the key already exists, it was created the first time the sketch was run.

   #. A key value can be retrieved regardless of the mode in which the namespace was opened, but a value can only be stored if the namespace is open in read-write mode.
   
   #. Data types of the “``get``'s” and “``put``'s” must match.
   
   #. Remember the 15 character limit for namespace and key names.


Real World Example
------------------

Here is part of a ``setup()`` function that uses Preferences.

Its purpose is to set either a factory default configuration if the system has never run before, or use the last configuration if it has.

When started, the system has no way of knowing which of the above conditions is true. So the first thing it does after opening the namespace is check for the existence of a key that we have predetermined can only exist if we have previously run the sketch. Based on its existence we decide if a factory default set of operating parameters should be used (and in so doing create the namespace keys and populate the values with defaults) or if we should use operating parameters from the last time the system was running.

.. code-block:: arduino
   
   #include <Preferences.h>
   
   #define RW_MODE false
   #define RO_MODE true
   
   Preferences stcPrefs;

   void setup() {
   
      // not the complete setup(), but in setup(), include this...
   
      stcPrefs.begin("STCPrefs", RO_MODE);           // Open our namespace (or create it
                                                     //  if it doesn't exist) in RO mode.
      
      bool tpInit = stcPrefs.isKey("nvsInit");       // Test for the existence
                                                     // of the "already initialized" key.

      if (tpInit == false) {
         // If tpInit is 'false', the key "nvsInit" does not yet exist therefore this 
         //  must be our first-time run. We need to set up our Preferences namespace keys. So...
         stcPrefs.end();                             // close the namespace in RO mode and...
         stcPrefs.begin("STCPrefs", RW_MODE);        //  reopen it in RW mode.
       

         // The .begin() method created the "STCPrefs" namespace and since this is our 
         //  first-time run we will create
         //  our keys and store the initial "factory default" values.
         stcPrefs.putUChar("curBright", 10);
         stcPrefs.putString("talChan", "one");
         stcPrefs.putLong("talMax", -220226);
         stcPrefs.putBool("ctMde", true);
         
         stcPrefs.putBool("nvsInit", true);          // Create the "already initialized"
                                                     //  key and store a value.
         
         // The "factory defaults" are created and stored so...
         stcPrefs.end();                             // Close the namespace in RW mode and...
         stcPrefs.begin("STCPrefs", RO_MODE);        //  reopen it in RO mode so the setup code
                                                     //  outside this first-time run 'if' block
                                                     //  can retrieve the run-time values
                                                     //  from the "STCPrefs" namespace. 
      }

      // Retrieve the operational parameters from the namespace
      //  and save them into their run-time variables.
      currentBrightness = stcPrefs.getUChar("curBright");  //  
      tChannel = stcPrefs.getString("talChan");            //  The LHS variables were defined
      tChanMax = stcPrefs.getLong("talMax");               //   earlier in the sketch.
      ctMode = stcPrefs.getBool("ctMde");                  //
      
      // All done. Last run state (or the factory default) is now restored.
      stcPrefs.end();                                      // Close our preferences namespace.
      
      // Carry on with the rest of your setup code...
      
      // When the sketch is running, it updates any changes to an operational parameter  
      //  to the appropriate key-value pair in the namespace.
      
   }


Utility Functions
-----------------

There are a few other functions useful when working with namespaces.

Deleting key-value Pairs
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: arduino

   preferences.clear();
   
.. 

      - Deletes *all* the key-value pairs in the currently opened namespace.
      
        - The namespace still exists.
      
        - The namespace must be open in read-write mode for this to work.

.. code-block:: arduino

   preferences.remove("keyname");
   
.. 

      - Deletes the "keyname" and value associated with it from the currently opened namespace.
      
        - The namespace must be open in read-write mode for this to work.
        - Tip: use this to remove the "test key" to force a "factory reset" during the next reboot (see the *Real World Example* above).

If either of the above are used, the ``key-value`` pair will need to be recreated before using it again.


Determining the Number of Available Keys
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For each namespace, Preferences keeps track of the keys in a key table. There must be an open entry in the table before a key can be created. This method will return the number of entires available in the table.

.. code-block:: arduino

   freeEntries()
   
.. 

To send to the serial monitor the number of available entries the following could be used.

.. code-block:: arduino

   Preferences mySketchPrefs;

   mySketchPrefs.begin("myPrefs", true);
   size_t whatsLeft = freeEntries();    // this method works regardless of the mode in which the namespace is opened.
   Serial.printf("There are: %u entries available in the namespace table.\n, whatsLeft);
   mySketchPrefs.end();

..

The number of available entries in the key table changes depending on the number of keys in the namespace and also the dynamic size of certain types of data stored in the namespace. Details are in the `Preferences API Reference`_.

Do note that the number of entries in the key table does not guarantee that there is room in the opened NVS namespace for all the data to be stored in that namespace. Refer to the espressif `Non-volatile storage library`_ documentation for full details.


Determining the Type of a key-value Pair
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Keeping track of the data types stored against a key-value pair is one of the bookkeeping tasks left to you. Should you want to discover the Preferences data type stored against a given key, use this method:

.. code-block:: arduino

   getType("myKey")

..

As in:

.. code-block:: arduino

   PreferenceType whatType = getType("myKey");
   
..

The value returned is a ``PreferenceType`` value that maps to a Preferences Type. Refer to the description in the `Preferences API Reference`_ for details.



Working with Large Data
-----------------------

Recall that the Preferences library works best for storing many small values, rather than a few large values. Regardless, it may be desirable to store larger amounts of arbitrary data than what is provided by the basic types in the Preferences Types table above.

The library provides the following methods to facilitate this.

.. code-block:: arduino

   putBytes("myBytesKey", value, valueLen)
   getBytes("myBytesKey", buffer, valueLen)
   getBytesLength("myBytesKey")
   
..

The ``put`` and ``get`` ``Bytes`` methods store and retrieve the data. The ``getBytesLength`` method is used to find the size of the data stored against the key (which is needed to retrieve ``Bytes`` data).

As the names of the methods imply, they operate on variable length bytes of data (often referred to as a "blob") and not on individual elements of a certain data type.

Meaning if you store for example an array of type ``int16_t`` against a ``Bytes`` type key, the value of that key becomes a series of bytes with no associated data type. Or if you like, all data stored as a blob gets converted to a series of ``uint8_t`` type bytes.

As a result, when using the ``getBytes`` method to retrieve the value of the key, what is returned to the buffer is a series of ``uint8_t`` bytes. It is up to you to manage the data types and size of the arrays and buffers when retrieving ``Bytes`` data.

Fortunately this is not as difficult as it may sound as the ``getBytesLength`` method and the ``sizeof`` operator help with keeping track of it all.

This is best explained with an example. Here the ``Bytes`` methods are used to store and retrieve an array, while ensuring the data type is preserved.

.. code-block:: arduino

   /*
    *  An example sketch using the Preferences "Bytes" methods
    *   to store and retrieve an arbitrary number of bytes in
    *   a namespace.
    */

   #include <Preferences.h>

   #define RO_MODE true
   #define RW_MODE false

   void setup() {

       Preferences mySketchPrefs;

       Serial.begin(115200);
       delay(250);
    
       mySketchPrefs.begin("myPrefs", RW_MODE);   // open (or create) the namespace
                                                  //  "myPrefs" in RW mode
       mySketchPrefs.clear();                     // delete any previous keys in this namespace
    
       // Create an array of test values. We're using hex numbers
       //  throughout to better show how the bytes move around.
       int16_t myArray[] = { 0x1112, 0x2122, 0x3132, 0x4142, 0x5152, 0x6162, 0x7172 };
    
       Serial.println("Printing myArray...");
       for (int i = 0; i < sizeof(myArray) / sizeof(int16_t); i++) {
           Serial.print(myArray[i], HEX); Serial.print(", ");
       }
       Serial.println("\r\n");
    
       // In the next statement, the second sizeof() needs
       //  to match the data type of the elements of myArray
       Serial.print("The number of elements in myArray is: ");
       Serial.println( sizeof(myArray) / sizeof(int16_t) );
       Serial.print("But the size of myArray in bytes is: ");
       Serial.println( sizeof(myArray) );
       Serial.println("");
    
       Serial.println(
         "Storing myArray into the Preferences namespace \"myPrefs\" against the key \"myPrefsBytes\".");
       // Note: in the next statement, to store the entire array, we must use the 
       //  size of the arrray in bytes, not the number of elements in the array.
       mySketchPrefs.putBytes( "myPrefsBytes", myArray, sizeof(myArray) );
       Serial.print("The size of \"myPrefsBytes\" is (in bytes): ");
       Serial.println( mySketchPrefs.getBytesLength("myPrefsBytes") );
       Serial.println("");
    
       int16_t myIntBuffer[20] = {}; // No magic about 20. Just making a buffer (array) big enough.
       Serial.println("Retrieving the value of myPrefsBytes into myIntBuffer.");
       Serial.println("   - Note the data type of myIntBuffer matches that of myArray");
       mySketchPrefs.getBytes("myPrefsBytes", myIntBuffer,
                              mySketchPrefs.getBytesLength("myPrefsBytes"));
    
       Serial.println("Printing myIntBuffer...");
       // In the next statement, sizeof() needs to match the data type of the elements of myArray
       for (int i = 0; i < mySketchPrefs.getBytesLength("myPrefsBytes") / sizeof(int16_t); i++) {
          Serial.print(myIntBuffer[i], HEX); Serial.print(", ");
       }
       Serial.println("\r\n");

       Serial.println(
         "We can see how the data from myArray is actually stored in the namespace as follows.");
       uint8_t myByteBuffer[40] = {}; // No magic about 40. Just making a buffer (array) big enough.
       mySketchPrefs.getBytes("myPrefsBytes", myByteBuffer,
                              mySketchPrefs.getBytesLength("myPrefsBytes"));
    
       Serial.println("Printing myByteBuffer...");
       for (int i = 0; i < mySketchPrefs.getBytesLength("myPrefsBytes"); i++) {
          Serial.print(myByteBuffer[i], HEX); Serial.print(", ");
       }
       Serial.println("");

   }

   void loop() {
     ;
   }

..

The resulting output is:
::

   Printing myArray...
   1112, 2122, 3132, 4142, 5152, 6162, 7172, 

   The number of elements in myArray is: 7
   But the size of myArray in bytes is: 14

   Storing myArray into the Preferences namespace "myPrefs" against the key "myPrefsBytes".
   The size of "myPrefsBytes" is (in bytes): 14

   Retrieving the value of myPrefsBytes into myIntBuffer.
      - Note the data type of myIntBuffer matches that of myArray
   Printing myIntBuffer...
   1112, 2122, 3132, 4142, 5152, 6162, 7172, 

   We can see how the data from myArray is actually stored in the namespace as follows.
   Printing myByteBuffer...
   12, 11, 22, 21, 32, 31, 42, 41, 52, 51, 62, 61, 72, 71, 

You can copy the sketch and change the data type and values in ``myArray`` and follow along with the code and output to see how the ``Bytes`` methods work. The data type of ``myIntBuffer`` should be changed to match that of ``myArray`` (and check the "``sizeof()``'s" where indicated in the comments).

The main takeaway is to remember you're working with bytes and so attention needs to be paid to store all the data based on the size of its type and to manage the buffer size and data type for the value retrieved.


Multiple Namespaces
-------------------

As stated earlier, multiple namespaces can exist in the Preferences NVS partition. However, only one namespace at a time can be open (in use).

If you need to access a different namespace, close the one before opening the other. For example:

.. code-block:: arduino

   Preferences currentNamespace;

      currentNamespace.begin("myNamespace", false);
         // do stuff...

      currentNamespace.end();                              // closes 'myNamespace' 
   
      currentNamespace.begin("myOtherNamespace", false);   // opens a different Preferences namesspace.
         // do other stuff...
         
      currentNamespace.end();                              // closes 'myOtherNamespace'

Here the "``currentNamespace``" object is reused, but different Preferences objects can be declared and used. Just remember to keep it all straight as all "``putX``'s" and "``getX``'s", etc. will only operate on the single currently opened namespace.


A Closer Look at ``getX`` 
--------------------------

Methods in the Preferences library return a status code that can be used to determine if the method completed successfully. This is described in the `Preferences API Reference`_.

Assume we have a key named "``favourites``" that contains a value of a ``String`` data type.

After executing the statement:

.. code-block:: arduino

  dessert = mySketchPrefs.getString("favourites");
  
..

the variable ``dessert`` will contain the value of the string stored against the key ``"favourites"``.

But what if something went wrong and the ``getString`` call failed to retrieve the key value? How would we be able to detect the error?

With Preferences, the ``getX`` methods listed in Table 2 below will return a default value if an error is encountered.

.. table:: **Table 2 — getX Methods Defaults**
   :align: center

   +------------------+-----------------+
   | Preferences      | Default Return  |
   | Type             | Value           |
   +==================+=================+
   | Char, UChar,     | 0               |
   |                  |                 |
   | Short, UShort,   |                 |
   |                  |                 |
   | Int, UInt,       |                 |
   |                  |                 |
   | Long, ULong,     |                 |
   |                  |                 |
   | Long64, ULong64  |                 |
   +------------------+-----------------+
   | Bool             | false           |
   +------------------+-----------------+
   | Float            | NAN             |
   |                  |                 |
   | Double           |                 |
   +------------------+-----------------+
   | String (String)  | ""              |
   +------------------+-----------------+
   | String (* buf)   | \\0             |
   +------------------+-----------------+

Thus to detect an error we could compare the value returned against its default return value and if they are equal assume an error occurred and take the appropriate action.

But what if a method default return value is also a potential legitimate value? How can we then know if an error occurred?

As it turns out, the complete form of the ``getX`` methods for each of the Preferences Types in Table 2 is:

.. code-block:: arduino

   preferences.getX("myKey", myDefault)

..

In this form the method will return either the value associated with "``myKey``" or, if an error occurred, return the value ``myDefault``, where ``myDefault`` must be the same data type as the ``getX``.

Returning to the example above:

.. code-block:: arduino

  dessert = mySketchPrefs.getString("favourites", "gravel");

..

will assign to the variable ``dessert`` the String ``gravel`` if an error occurred, or the value stored against the key ``favourites`` if not.

If we predetermine a default value that is outside all legitimate values, we now have a way to test if an error actually occurred.

In summary, if you need to confirm that a value was retrieved without error from a namespace, use the complete form of the ``getX`` method with a predetermined default "this can only happen if an error" value and compare that against the value returned by the call. Otherwise, you can omit the default value as the call will return the default for that particular ``getX`` method.

Additional detail is given in the `Preferences API Reference`_.


Advanced Item
-------------

In the arduino-esp32 implementation of Preferences there is no method to completely remove a namespace. As a result, over the course of a number of projects, it is possible that the ESP32 NVS Preferences partition becomes cluttered or full.

To completely erase and reformat the NVS memory used by Preferences, create and run a sketch that contains:

.. code-block:: arduino

   #include <nvs_flash.h>

   void setup() {

       nvs_flash_erase();      // erase the NVS partition and...
       nvs_flash_init();       // initialize the NVS partition.
       while (true);

   }

   void loop() {
      ;
   }

.. 

.. warning:: 
   **You should download a new sketch to your board immediately after running the above or else it will reformat the NVS partition every time it is powered up or restarted!**


Resources
---------

* `Preferences API Reference <../api/preferences.html>`_
* `Non-volatile storage library`_ (espressif-IDF API Reference)
* `Official ESP-IDF documentation`_ (espressif-IDF Reference)


.. _Non-volatile storage library: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html
.. _Official ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/en/stable


Contribute
----------

.. ==*Do not change! Keep as is.*==

To contribute to this project, see `How to contribute`_.

If you have any **feedback** or **issue** to report on this tutorial, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try the Troubleshooting and to check if the same issue was already created by someone else.

.. _How to Contribute: https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst

.. ---- EOF ----
