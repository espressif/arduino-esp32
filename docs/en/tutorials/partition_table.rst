###############
Partition Table
###############

Introduction
------------

Partition table is used to define the flash memory organization and the different kind of data will be stored on each partition.

You can use one of the available partition table scheme or create your own. You can see all the different schemes on the `tools/partitions <https://github.com/espressif/arduino-esp32/tree/master/tools/partitions>`_ folder or by the Arduino IDE tools menu `Tools -> Partition Scheme`.

The partition table is created by a .CSV (Comma-separated Values) file with the following structure:

.. code-block::

    # ESP-IDF Partition Table
    # Name, Type, SubType, Offset, Size, Flags

Where:

1. **Name**

    Is the partition name and must be a unique name. This name is not relevant for the system and the size must be at maximum of 16-chars (no special chars).

2. **Type**

    This is the type of the partition. This value can be ``data`` or ``app``.

    * ``app`` type is used to define the partition that will store the application.

    * ``data`` type can be used to define the partition that stores general data, not the application.

3. **SubType**

    The SubType defines the usage of the ``app`` and ``data`` partitions.

   **data**

       ``ota``

           The ota subtype is used to store the OTA information. This partition is used only when the OTA is used to select the initialization partition, otherwise no need to add it to your custom partition table.
           The size of this partition should be a fixed size of 8kB (0x2000 bytes).

       ``nvs``

           The nvs partition subtype is used to define the partition to store general data, like the WiFi data, device PHY calibration data and any other data to be stored on the non-volatile memory.
           This kind of partition is suitable for small custom configuration data, cloud certificates, etc. Another usage for the NVS is to store sensitive data, since the NVS supports encryption.
           It is highly recommended to add at least one nvs partition, labeled with the name nvs, in your custom partition tables with size of at least 12kB (0x3000 bytes). If needed, you can increase the size of the nvs partition.
           The recommended size for this partition is from 12kb to 64kb. Although larger NVS partitions can be defined, we recommend using FAT or SPIFFS filesystem for storage of larger amounts of data.

       ``coredump``

           The coredump partition subtype is used to store the core dump on the flash. The core dump is used to analyze critical errors like crash and panic.
           This function must be enabled in the project configuration menu and set the data destination to flash.
           The recommended size for this partition is 64kB (0x10000).

       ``nvs_keys``

           The nvs_keys partition subtype is used to store the keys when the NVS encryption is used.
           The size for this partition is 4kB (0x1000).

       ``fat``

           The fat partition subtype defines the FAT filesystem usage, and it is suitable for larger data and if this data is often updated and changed. The FAT FS can be used with wear leveling feature to increase the erase/modification cycles per memory sector and encryption for sensitive data storage, like cloud certificates or any other data that may be protected.
           To use FAT FS with wear leveling see the example.

       ``spiffs``

           The spiffs partition subtype defines the SPI flash filesystem usage, and it is also suitable for larger files and it also performs the wear leveling and file system consistency check.
           The SPIFFS do not support flash encryption.

   **app**

    ``factory``

        The factory partition subtype is the default application. The bootloader will set this partition as the default application initialization if no OTA partition is found, or the OTA partitions are empty.
        If the OTA partition is used, the ota_0 can be used as the default application and the factory can be removed from the partition table to save memory space.

    ``ota_0`` to ``ota_15``

        The ota_x partition subtype is used for the Over-the air update. The OTA feature requires at least two ota_x partition (usually ota_0 and ota_1) and it also requires the ota partition to keep the OTA information data.
        Up to 16 OTA partitions can be defined but only two are needed for basic OTA feature.

    ``test``

        The test partition subtype is used for factory test procedures.

4. **Offset**

    The offset defines the partition start address. The offset is defined by the sum of the offset and the size of the earlier partition.

.. note::
    Offset must be multiple of 4kB (0x1000) and for app partitions it must be aligned by 64kB (0x10000).
    If left blank, the offset will be automatically calculated based on the end of the previous partition, including any necessary alignment, however, the offset for the first partition must be always set as **0x9000** and for the first application partition **0x10000**.

5. **Size**

    Size defines the amount of memory to be allocated on the partition. The size can be formatted as decimal, hex numbers (0x prefix), or using unit prefix K (kilo) or M (mega) i.e: 4096 = 4K = 0x1000.

6. **Flags**

    The last column in the CSV file is the flags and it is currently used to define if the partition will be encrypted by the flash encryption feature.


For example, **the most common partition** is the ``default_8MB.csv`` (see `tools/partitions <https://github.com/espressif/arduino-esp32/tree/master/tools/partitions>`_ folder for some examples):

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     0x9000,  0x5000,
    otadata,  data, ota,     0xe000,  0x2000,
    app0,     app,  ota_0,   0x10000, 0x330000,
    app1,     app,  ota_1,   0x340000,0x330000,
    spiffs,   data, spiffs,  0x670000,0x190000,

Using a Custom Partition Scheme
-------------------------------

To create your own partition table, you can create the ``partitions.csv`` file **in the same folder you created your sketch**. The build system will automatically pick the partition table file and use it instead of the predefined ones.

Here is an example you can use for a custom partition table:

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     36K,     20K,
    otadata,  data, ota,     56K,     8K,
    app0,     app,  ota_0,   64K,     2M,
    app1,     app,  ota_1,   ,        2M,
    spiffs,   data, spiffs,  ,        8M,

This partition will use about 12MB of the 16MB flash. The offset will be automatically calculated after the first application partition and the units are in K and M.

An alternative is to create the new partition table as a new file in the `tools/partitions <https://github.com/espressif/arduino-esp32/tree/master/tools/partitions>`_ folder and edit the `boards.txt <https://github.com/espressif/arduino-esp32/tree/master/boards.txt>`_ file to add your custom partition table.

Another alternative is to create the new partition table as a new file, and place it in the `variants <https://github.com/espressif/arduino-esp32/tree/master/variants>`_ folder under your boards folder, and edit the `boards.txt <https://github.com/espressif/arduino-esp32/tree/master/boards.txt>`_ file to add your custom partition table, noting that in order for the compiler to find your custom partition table file you must use the '.build.custom_partitions=' option in the boards.txt file, rather than the standard '.build.partitions=' option. The '.build.variant=' option has the name of the folder holding your custom partition table in the variants folder.

An example of the PartitionScheme listing using the ESP32S3 Dev Module as a reference, would be to have the following:

**Custom Partition - CSV file in /variants/custom_esp32s3/ folder**

.. code-block::

    esp32s3.build.variant=custom_esp32s3
    --
    esp32s3.menu.PartitionScheme.huge_app=Custom Huge APP (3MB No OTA/1MB SPIFFS)
    esp32s3.menu.PartitionScheme.huge_app.build.custom_partitions=custom_huge_app
    esp32s3.menu.PartitionScheme.huge_app.upload.maximum_size=3145728

Examples
--------

**2MB no OTA**

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     36K,     20K,
    factory,  app,  factory, 64K,     1900K,

**4MB no OTA**

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     36K,     20K,
    factory,  app,  factory, 64K,     4000K,

**4MB with OTA**

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     36K,     20K,
    otadata,  data, ota,     56K,     8K,
    app0,     app,  ota_0,   64K,     1900K,
    app1,     app,  ota_1,   ,        1900K,

**8MB no OTA with Storage**

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     36K,     20K,
    factory,  app,  factory, 64K,     2M,
    spiffs,   data, spiffs,  ,        5M,

**8MB with OTA and Storage**

.. code-block::

    # Name,   Type, SubType, Offset,  Size, Flags
    nvs,      data, nvs,     36K,     20K,
    otadata,  data, ota,     56K,     8K,
    app0,     app,  ota_0,   64K,     2M,
    app1,     app,  ota_1,   ,        2M,
    spiffs,   data, spiffs,  ,        3M,

Reference
---------

This documentation was based on the `How to use custom partition tables on ESP32 <https://medium.com/p/69c0f3fa89c8>`_ article.
