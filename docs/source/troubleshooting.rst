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

* In some instances, you must keep **GPIO0** LOW during the uploading process via serial interface.
* Hold-down the **“BOOT”** button in your ESP32 board while uploading/flashing.
* In some ESP32 development boards when trying to upload a new sketch, the Arduino IDE gives you a fatal error *(i.e A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header).* This means your ESP32 board is not automatically in the flashing/uploading mode.
**Solution:** 

1. To automatically get into the flashing mode, you have to connect a **10 uF** capacitor between the **EN** and **GND** pins. This capacitor value could be lower (> 2.2 uF) if you don't have this part available. **EN <----||---- GND.** *(The capacitor can be electrolytic, ceramic, or tantalum. Using lower values like 2.2uF and 4.7 uF is also possible in most cases.)*
2. The hardware guide recommends adding the RC delay circuit. For more details, see the **ESP32 Hardware Design Guidelines**
  https://www.espressif.com/sites/default/files/documentation/esp32_hardware_design_guidelines_en.pdf 
   
  ``in the '2.2.1 Power-on Sequence' section.``

Hardware
--------

* Power Source.
* Bad/damaged USB cable or charging only cables (without data wires).
* USB drivers missing.
* Board with some defect.
