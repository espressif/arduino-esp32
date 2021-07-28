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

Why is my board not flashing/uploading when I try to upload my sketch?
**********************************************************************

If you are trying to upload a new sketch and your board isn't responding, there are some possible reasons.
To be able to upload the sketch via serial interface, the ESP32 must be in the download mode. The download mode allows you to upload the sketch over the serial port and to get into it, you need to keep the **GPIO0** in LOW while a resetting (**EN** pin) cycle.

Possible fatal error message from the Arduino IDE:

    *A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header*

Here are some steps that you can try to:

* Check your USB cable and try a new one.
* Change the USB port.
* Check your power supply.
* In some instances, you must keep **GPIO0** LOW during the uploading process via serial interface.
* Hold-down the **“BOOT”** button in your ESP32 board while uploading/flashing.

In some development boards, you can try adding the reset delay circuit, as decribed in the *Power-on Sequence* section on the `ESP32 Hardware Design Guidelines <https://www.espressif.com/sites/default/files/documentation/esp32_hardware_design_guidelines_en.pdf>`_ in order to get into the download mode automatically.

Hardware
--------

Why is my computer not detecting my board?
**************************************************

If your board is not detected after connecting into the USB, you can try to:

* Check if the USB driver is missing. - `USB Driver Download Link  <https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers>`_
* Check your USB cable and try a new one.
* Change the USB port.
* Check your power supply.
* Check if the board is damaged or defective.
