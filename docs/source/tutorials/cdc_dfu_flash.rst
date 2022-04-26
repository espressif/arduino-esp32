########################
USB CDC and DFU Flashing
########################

Introduction
------------

Since the ESP32-S2 introduction, Espressif has been working on USB peripheral support for some of the SoC families, including the ESP32-C3 and the ESP32-S3.

This new peripheral allows a lot of new possibilities, including flashing the firmware directly to the SoC without any external USB-to-Serial converter.

In this tutorial, we will guide you on how to use the embedded USB to flash the firmware.

**The current list of supported SoCs:**

========= =======================
SoC       USB Peripheral Support
========= =======================
ESP32-S2  CDC and DFU
ESP32-C3  CDC
ESP32-S3  CDC and DFU
========= =======================

USB DFU
-------

Flashing Using DFU
******************

.. note::
    DFU is only supported by the ESP32-S2.

USB CDC
-------

Flashing Using CDC
******************

Hardware
--------

