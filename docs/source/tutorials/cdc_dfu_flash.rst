########################
USB CDC and DFU Flashing
########################

Introduction
------------

Since the ESP32-S2 introduction, Espressif has been working on USB peripheral support for some of the SoC families, including the ESP32-C3 and the ESP32-S3.

This new peripheral allows a lot of new possibilities, including flashing the firmware directly to the SoC without any external USB-to-Serial converter.

In this tutorial, you will be guided on how to use the embedded USB to flash the firmware.

**The current list of supported SoCs:**

========= =======================
SoC       USB Peripheral Support
========= =======================
ESP32-S2  CDC and DFU
ESP32-C3  CDC only
ESP32-S3  CDC and DFU
========= =======================

It's important that your board includes the USB connector attached to the embedded USB from the SoC. If your board doesn't have the USB connector, you can attach an external one to the USB pins.

These instructions will only work on the supported devices with the embedded USB peripheral. This tutorial will not work if you are using an external USB-to-serial converter like FTDI, CP210x, CH340, etc.

For a complete reference to the Arduino IDE tools menu, please see the `Tools Menus <../guides/tools_menu.html>`_ reference guide.

USB DFU
-------

The USB DFU (Device Firmware Upgrade) is a class specification from the USB standard that adds the ability to upgrade the device firmware by the USB interface.

Flashing Using DFU
******************

.. note::
    DFU is only supported by the ESP32-S2 and ESP32-S3. See the table of supported SoCs.

To use the USB DFU to flash the device, you will need to configure some settings in the Arduino IDE according to the following steps:

1. Enter into Download Mode manually

This step is done only for the first time you flash the firmware in this mode. To enter into the download mode, you need to press and hold BOOT button and press and release the RESET button.

To check if this procedure was done correctly, now you will see the new USB device listed in the available ports. Select this new device in the **Port** option.

2. Configure the USB DFU

In the next step you can set the USB DFU as default on BOOT and for flashing.

Go to the Tools menu in the Arduino IDE and set the following options:

**For ESP32-S2**

* USB DFU On Boot -> Enable

* Upload Mode -> Internal USB

**For ESP32-S3**

* USB Mode -> USB-OTG (TinyUSB)

* USB DFU On Boot -> Enabled

Setp 3 - Flash
^^^^^^^^^^^^^^

Now you can upload your sketch to the device. After flashing, you need to manually reset the device.

.. note::
        On the USB DFU, you can't use the USB for the serial output for the logging, just for flashing. To enable the serial output, use the CDC option instead.
        If you want to use the USB DFU for just upgrading the firmware using the manual download mode, this will work just fine, however, for developing please consider using USB CDC.


USB CDC
-------

The USB CDC (Communications Device Class) allows you to communicate to the device like in a serial interface. This mode can be used on the supported targets to flash and monitor the device in a similar way on devices that uses the external serial interfaces.

To use the USB CDC, you need to configure your device in the Tools menu:


1. Enter into Download Mode manually

Similar to the DFU mode, you will need to enter into download mode manually. To enter into the download mode, you need to press and hold BOOT button and press and release the RESET button.

To check if this procedure was done correctly, now you will see the new USB device listed in the available ports. Select this new device in the **Port** option.

2. Configure the USB CDC

**For ESP32-S2**

* USB CDC On Boot -> Enabled

* Upload Mode -> Internal USB

**For ESP32-C3**

* USB CDC On Boot -> Enabled

**For ESP32-S3**

* USB CDC On Boot -> Enabled

* Upload Mode -> UART0 / Hardware CDC

3. Flash and Monitor

You can now upload your sketch to the device. After flashing for the first time, you need to manually reset the device.

This procedure enables the flashing and monitoring thought the internal USB and does not requires you to manually enter into the download mode or to do the manual reset after flashing.

To monitor the device, you need to select the USB port and open the Monitor tool selecting the correct baud rate (usually 115200) according to the ``Serial.begin()`` defined in your code.

Hardware
--------

If you are developing a custom hardware using the compatible SoC, and want to remove the external USB-to-Serial chip, this feature will complete substitute the needs of the external chip. See the SoC datasheet for more details about this peripheral.
