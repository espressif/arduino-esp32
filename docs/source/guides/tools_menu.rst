######################
Arduino IDE Tools Menu
######################

Introduction
------------

This guide is a walkthrough for the Arduino IDE configuration menu for the ESP32 SoC's. In this guide, you will see the most relevant configuration
to get your project optimized and working.

Since some boards and SoC's may vary in terms of hardware configuration, be sure you know all the board characteristics that you are using, like flash memory size, SoC variant (ESP32 family), PSRAM, etc.

.. note:: To help you identify the characteristics, you can see the boards section and the Espressif Product Selector.

Arduino IDE
-----------



Tools Menu
----------

To properly configure your project build and flash, some settings must be done in order to get it compiled and flashed without any issues.
Some boards are natively supported and almost no configuration is required. However, if your is not yet supported or you have a custom board, you need to configure the environment by yourself.

Generic Options
---------------

Board
*****

This option is the target board and must be selected in order to get all the default configuration settings. Once you select the correct board, you will see that some configurations will be automatically selected, but be aware that some boards can have multiple versions (i.e different flash sizes).

To select the board, go to ``Tools -> Board -> ESP32 Arduino`` and select the target board.

If your board is not present on this list, you can select the generic ``ESP32-XX Dev Module``.

Currently we have one generic development module for each of the supported targets.

Upload Speed
************

To select the flashing speed, change the ``Tools -> Upload Speed``. This value will be used for flashing the code to the device.

.. note:: If you have issues while flashing the device at high speed, try to decrease this value. This could be due the external serial-to-USB chip limitations.

CPU Frequency
*************

On this option, you can select the CPU clock frequency. This option is critical and must be selected according to the high frequency crystal present on the board and the radio usage (Wi-Fi and Bluetooth).

Flash Frequency
***************

Flash Mode
**********

Flash Size
**********

Partition Scheme
****************

Core Debug Level
****************

PSRAM
*****

Arduino Runs On
***************

Events Run On
*************

Port
****

Get Board Info
**************

USB Options
-----------

Some ESP32 families have the USB peripheral. This peripheral can be used also for flashing and debigging.

Currently, the SoC's with USB supported are:

* ESP32-C3
* ESP32-S2
* ESP32-S3 (in development mode, not stable yet)

The USB option will be available only if the correct target is selected.

USB CDC On Boot
***************

The USB Communications Device Class, or USB CDC, is a class used for basic communication to be used as a regular serial controller (like RS-232).

This class is used for flashing the device without any other external device attached to the SoC.

This option can be used to ``Enable`` or ``Disable`` this function at the boot. If this option is ``Enabled``, once the device is connected via USB, one new serial port will appear in the serial ports list.
Use this new serial port for flashing the device.

USB Firmware MSC On Boot
************************

The USB Mass Storage Class

USB DFU On Boot
***************
