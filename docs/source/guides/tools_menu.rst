######################
Arduino IDE Tools Menu
######################

Introduction
------------

This guide is a walkthrough of the Arduino IDE configuration menu for the ESP32 System on Chip (SoC's). In this guide, you will see the most relevant configuration
to get your project optimized and working.

Since some boards and SoC's may vary in terms of hardware configuration, be sure you know all the board characteristics that you are using, like flash memory size, SoC variant (ESP32 family), PSRAM, etc.

.. note:: To help you identify the characteristics, you can see the `Espressif Product Selector`_.

Arduino IDE
-----------

The Arduino IDE is widely used for ESP32 on Arduino development and offers a wide variety of configurations.

Tools Menu
----------

To properly configure your project build and flash, some settings must be done in order to get it compiled and flashed without any issues.
Some boards are natively supported and almost no configuration is required. However, if your is not yet supported or you have a custom board, you need to configure the environment by yourself.

For more details or to add a new board, see the `boards.txt`_ file.

Generic Options
---------------

Most of the options are available for every ESP32 families. Some options will be available only for specific targets, like the USB configuration.

Board
*****

This option is the target board and must be selected in order to get all the default configuration settings. Once you select the correct board, you will see that some configurations will be automatically selected, but be aware that some boards can have multiple versions (i.e different flash sizes).

To select the board, go to ``Tools -> Board -> ESP32 Arduino`` and select the target board.

If your board is not present on this list, you can select the generic ``ESP32-XX Dev Module``.

Currently, we have one generic development module for each of the supported targets.

If the board selected belongs to another SoC family, you will see the following information at the build output:

    ``A fatal error occurred: This chip is ESP32 not ESP32-S2. Wrong --chip argument?``

Upload Speed
************

To select the flashing speed, change the ``Tools -> Upload Speed``. This value will be used for flashing the code to the device.

.. note:: If you have issues while flashing the device at high speed, try to decrease this value. This could be due to the external serial-to-USB chip limitations.

CPU Frequency
*************

On this option, you can select the CPU clock frequency. This option is critical and must be selected according to the high-frequency crystal present on the board and the radio usage (Wi-Fi and Bluetooth).

If you don't know why you shuld change this frequency, leave the default option.

Flash Frequency
***************

Use this function to select the flash memory frequency.

* **40MHz**
* **80MHz**

Flash Mode
**********

This option is used to select the SPI mode for the flash memory.

* **QIO** - Quad I/O Fast Read
    * Four SPI pins are used to write the flash address part of the command and to read flash data out.

* **DIO** - Dual I/O Fast Read
    * Two SPI pins are used to write the flash address part of the command and to read flash data out.

* **QOUT** - Quad Output Fast Read
    * Four SPI pins are used to read the flash data out.

* **DOUT** - Dual Output Fast Read
    * Two SPI pins are used to read flash data out.

If you don't know how the board flash is physically connected or the flash memory model, try the **QIO/QOUT** first and then **DIO/DOUT**.

Flash Size
**********

This option is used to select the flash size.

* **2MB** (16Mb)
* **4MB** (32Mb)
* **8MB** (64Mb)
* **16MB** (128Mb)

Partition Scheme
****************

This option is used to select the partition model according to the flash size and the resources needed, like storage area and OTA (Over The Air updates).

.. note:: Be careful selecting the right partition according to the flash size.

Core Debug Level
****************

This option is used to select the Arduino core debugging level to be printed to the serial debug.

* **None** - Prints nothing.
* **Error** - Onle at error level.
* **Warning** - Only at warning level and above.
* **Info** - Only at info level and above.
* **Debug** - Only at debug level and above.
* **Verbose** - Prints everything.

PSRAM
*****

The PSRAM is an internal or external extended RAM present on some boards, modules or SoC..

This option can be used to ``Enable`` or ``Disable`` the PSRAM.

Arduino Runs On
***************

This function is used to select the core that runs the Arduino core. This is only valid if the target SoC has 2 cores.

Events Run On
*************

This function is used to select the core that runs the events. This is only valid if the target SoC has 2 cores.

Port
****

This option is used to select the serial port to be used on the flashing and monitor.

USB Options
-----------

Some ESP32 families have a USB peripheral. This peripheral can be used for flashing and debugging.

Currently, the SoC's with USB supported are:

* ESP32-C3 (CDC only)
* ESP32-S2
* ESP32-S3 (in development mode, not stable yet)

The USB option will be available only if the correct target is selected.

USB CDC On Boot
***************

The USB Communications Device Class, or USB CDC, is a class used for basic communication to be used as a regular serial controller (like RS-232).

This class is used for flashing the device without any other external device attached to the SoC.

This option can be used to ``Enable`` or ``Disable`` this function at the boot. If this option is ``Enabled``, once the device is connected via USB, one new serial port will appear in the list of the serial ports.
Use this new serial port for flashing the device.

This option can be used as well for debugging via the ``Serial Monitor``. 

USB Firmware MSC On Boot
************************

The USB Mass Storage Class, or USB MSC, is a class used for storage devices, like a USB flash drive.

This option can be used to ``Enable`` or ``Disable`` this function at the boot. If this option is ``Enabled``, once the device is connected via USB, one new storage device will appear in the system as a storage drive.
Use this new storage drive to write or read files, or to drop a new firmware binary to flash the device.

.. figure:: ../_static/usb_msc_drive.png
    :align: center
    :width: 720
    :figclass: align-center

USB DFU On Boot
***************

The USB Device Firmware Upgrade is a class used for flashing the device through USB.

This option can be used to ``Enable`` or ``Disable`` this function at the boot. If this option is ``Enabled``, once the device is connected via USB, the device will appear as a USB DFU capable device.


.. _Espressif Product Selector: https://products.espressif.com/
.. _boards.txt: https://github.com/espressif/arduino-esp32/blob/master/boards.txt