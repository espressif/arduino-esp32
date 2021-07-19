###############
Troubleshooting
###############

Common Issues
=============

Here are some of the most common issues around the ESP32 development using Arduino.

.. note:: Please consider contributing if you have found any issues with the solution here.

Installing
----------

Building
--------

Flashing
--------

*Here are some of the tips if The board is not flashing..*

* Sometimes to program ESP32 via serial you must keep GPIO0 LOW during the programming process.
* Hold-down the **“BOOT”** button in your ESP32 board while unloading.
* In some ESP32 development boards when trying to upload a new sketch, the Arduino IDE gives you a fatal error *(i.e A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header).* This means your ESP32 board is not automatically in the flashing/uploading mode.
**Solution:** 
To make flashing automatically, you have to connect a **10 uF** capacitor between the **EN** and **GND** pins. This capacitor value could be lower (> 2.2 uF) if you don't have this part available. **EN <----||---- GND.**

*(The capacitor can be electrolytic, ceramic, or tantalum. Using lower values like 2.2uF and 4.7 uF is also possible in most cases.)*



Hardware
--------

* Power Source.
* Bad USB cable or charging only cables (without data wires.)
* USB drivers missing.
* Board with some defect.
