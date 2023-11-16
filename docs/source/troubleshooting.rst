###############
Troubleshooting
###############

Common Issues
=============

Here are some of the most common issues around the ESP32 development using Arduino.

.. note:: Please consider contributing if you have found any issues with the solution here.

Installing
----------

Here are the common issues during the installation.

Building
--------

Missing Python: "python": executable file not found in $PATH
************************************************************

You are trying to build your sketch using Ubuntu and this message appears:

.. code-block:: bash

    "exec: "python": executable file not found in $PATH
    Error compiling for board ESP32 Dev Module"

Solution
^^^^^^^^

To avoid this error, you can install the ``python-is-python3`` package to create the symbolic links.

.. code-block:: bash

    sudo apt install python-is-python3

If you are not using Ubuntu, you can check if you have the Python correctly installed or the presence of the symbolic links/environment variables.

Flashing
--------

Why is my board not flashing/uploading when I try to upload my sketch?
**********************************************************************

To be able to upload the sketch via the serial interface, the ESP32 must be in the download mode. The download mode allows you to upload the sketch over the serial port, and to get into it, you need to keep the **GPIO0** in LOW while resetting (**EN** pin) the cycle.
If you are trying to upload a new sketch and your board is not responding, there are some possible reasons.

Possible fatal error message from the Arduino IDE:

    *A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header*

Solution
^^^^^^^^

Here are some steps that you can try:

* Check your USB cable and try a new one (some cables are only for charging and there is no data connection).
* Change the USB port - prefer direct connection to the computer and avoid USB hubs. Some USB ports may share the power source with other ports used, for example, for charging a phone.
* Check your power supply.
* Make sure that nothing is connected to pins labeled **TX** and **RX**. Please refer to the pin layout table - some TX and RX pins may not be labeled on the dev board.
* In some instances, you must keep **GPIO0** LOW during the uploading process via the serial interface.
* Hold down the **“BOOT”** button on your ESP32 board while uploading/flashing.
* Solder a **10uF** capacitor in parallel with **RST** and **GND**.
* If you are using external power connected to pins, it is easy to confuse pins **CMD** (which is usually next to the 5V pin) and **GND**.

In some development boards, you can try adding the reset delay circuit, as described in the *Power-on Sequence* section on the `ESP32 Hardware Design Guidelines <https://www.espressif.com/sites/default/files/documentation/esp32_hardware_design_guidelines_en.pdf>`_ to get into the download mode automatically.

Hardware
--------

Why is my computer not detecting my board?
******************************************

If your board is not being detected after connecting to the USB, you can try the following:

Solution
^^^^^^^^

* Check if the USB driver is missing. - `USB Driver Download Link  <https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers>`_
* Check your USB cable and try a new one.
* Change the USB port.
* Check your power supply.
* Check if the board is damaged or defective.

Wi-Fi
-----

Why does the board not connect to WEP/WPA-"encrypted" Wi-Fi?
************************************************************

Please note that WEP/WPA has significant security vulnerabilities, and its use is strongly discouraged.
The support may, therefore, be removed in the future. Please migrate to WPA2 or newer.

Solution
^^^^^^^^

Nevertheless, it may be necessary to connect to insecure networks. To do this, the security requirement of the ESP32 must be lowered to an insecure level by using:

.. code-block:: arduino

    WiFi.setMinSecurity(WIFI_AUTH_WEP); // Lower min security to WEP.
    // or
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK); // Lower min security to WPA.

Why does the board not connect to WPA3-encrypted Wi-Fi?
*******************************************************

WPA3 support is resource-intensive and may not be compiled into the used SDK.

Solution
^^^^^^^^

* Check WPA3 support by your SDK.
* Compile your custom SDK with WPA3 support.

Sample code to check SDK WPA3 support at compile time:

.. code-block:: arduino

    #ifndef CONFIG_ESP32_WIFI_ENABLE_WPA3_SAE
    #warning "No WPA3 support."
    #endif

Serial not printing
*******************

I have uploaded firmware to the ESP32 device, but I don't see any response from a Serial.print (HardwareSerial).

Solution
^^^^^^^^

Newer ESP32 variants have two possible USB connectors- USB and UART.  The UART connector will go through a USB->UART adapter, and will typically present itself with the name of that mfr (eg, Silicon Labs CP210x UART Bridge).  The USB connector can be used as a USB-CDC bridge and will appear as an Espressif device (Espressif USB JTAG/serial debug unit).  On Espressif devkits, both connections are available, and will be labeled.  ESP32 can only use UART, so will only have one connector.  Other variants with one connector will typically be using USB.  Please check in the product [datasheet](https://products.espressif.com) or [hardware guide](https://www.espressif.com/en/products/devkits) to find Espressif products with the appropriate USB connections for your needs.
If you use the UART connector, you should disable USB-CDC on boot under the Tools menu (-D ARDUINO_USB_CDC_ON_BOOT=0). If you use the USB connector, you should have that enabled (-D ARDUINO_USB_CDC_ON_BOOT=1) and set USB Mode to "Hardware CDC and JTAG" (-D ARDUINO_USB_MODE=0).
USB-CDC may not be able to initialize in time to catch all the data if your device is in a tight reboot loop. This can make it difficult to troubleshoot initialization issues.

SPIFFS mount failed
-------------------
When you come across an error like this:

.. code-block:: shell

   E (588) SPIFFS: mount failed, -10025
   [E][SPIFFS.cpp:47] begin(): Mounting SPIFFS failed! Error: -1

Try enforcing format on fail in your code by adding ``true`` in the ``begin`` method such as this:

.. code-block:: c++

   SPIFFS.begin(true);

See the method prototype for reference: ``bool begin(bool formatOnFail=false, const char * basePath="/spiffs", uint8_t maxOpenFiles=10, const char * partitionLabel=NULL);``

SD card mount fail
------------------
Even though you made sure that the pins are correctly connected, and not using restricted pins, you may still get an error such as this:

.. code-block:: shell

  [ 1065][E][sd_diskio.cpp:807] sdcard_mount(): f_mount failed: (3) The physical drive cannot work

Most of the problems originate from a poor connection caused by prototyping cables/wires, and one of the best solutions is to **solder all the connections** or use good quality connectors.

Note that with SD_MMC lib all the data pins need to be pulled up with an external 10k to 3.3V. This applies especially to card's D3 which needs to be pulled up even when using 1-bit line connection and the D3 is not used.

If you want to try the software approach before soldering, try manually specifying SPI pins, like this:

.. code-block:: c++

  int SD_CS_PIN = 19;
  SPI.begin(18, 36, 26, SD_CS_PIN);
  SPI.setDataMode(SPI_MODE0);
  SD.begin(SD_CS_PIN);


ESP32-S3 is rebooting even with a bare minimum sketch
*****************************************************
Some ESP32-S3 boards are equipped with Quad SPI (QSPI) or Octal SPI (OPI) PSRAM. If you upload such a board with default settings for ESP32-S3, it will result in rebooting with a message similar to this:

https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash_psram_config.html

.. code-block:: bash

    E (124) esp_core_dump_flash: Core dump flash config is corrupted! CRC=0x7bd5c66f instead of 0x0
    Rebooting...
    ⸮⸮⸮ESP-ROM:esp32s3-20210327
    Build:Mar 27 2021
    rst:0xc (RTC_SW_CPU_RST),boot:0x18 (SPI_FAST_FLASH_BOOT)
    Saved PC:0x40376af0
    SPIWP:0xee
    Octal Flash Mode Enabled
    For OPI Flash, Use Default Flash Boot Mode
    mode:SLOW_RD, clock div:1
    load:0x3fce3808,len:0x44c
    load:0x403c9700,len:0xbec
    load:0x403cc700,len:0x2920
    entry 0x403c98d8

    assert failed: do_core_init startup.c:326 (flash_ret == ESP_OK)


To fix the issue, you will need to find out the precise module you are using and set **PSRAM** in the Arduino IDE Tools according to the following table.

How to determine the module version:
------------------------------------

* First determine if you have a `WROOM-1 <https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf>`_ or `WROOM-2 <https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-2_datasheet_en.pdf>`_ module - this is written on the module shielding almost at the top, right under the ESP logo and company name (Espresif) right after the ESP32-S3 - for example ESP32-S3-WROOM-2.
* Then locate the version code on left bottom corner on the module shielding. The markings are very small and it might be really difficult to read with naked eyes - try using a camera with careful lighting.

With this knowledge find your module in the table and note what is written in the **PSRAM** column.

- If the results is empty (-) you don't need to change anything
- For QSPI go to Tools > PSRAM > QSPI PSRAM
- For OPI go to Tools > PSRAM > OPI PSRAM

Note that WROOM-2 has always OPI.

+---------+--------+------------+-------+
| Module  | Code   | Flash Mode | PSRAM |
+=========+========+============+=======+
| WROOM-1 | N4     | QSPI       | -     |
+---------+--------+------------+-------+
| WROOM-1 | N8     | QSPI       | -     |
+---------+--------+------------+-------+
| WROOM-1 | N16    | QSPI       | -     |
+---------+--------+------------+-------+
| WROOM-1 | H4     | QSPI       | -     |
+---------+--------+------------+-------+
| WROOM-1 | N4R2   | QSPI       | QSPI  |
+---------+--------+------------+-------+
| WROOM-1 | N8R2   | QSPI       | QSPI  |
+---------+--------+------------+-------+
| WROOM-1 | N16R2  | QSPI       | QSPI  |
+---------+--------+------------+-------+
| WROOM-1 | N4R8   | QSPI       | OPI   |
+---------+--------+------------+-------+
| WROOM-1 | N8R8   | QSPI       | OPI   |
+---------+--------+------------+-------+
| WROOM-1 | N16R8  | QSPI       | OPI   |
+---------+--------+------------+-------+
| WROOM-2 | N16R8V | OPI        | OPI   |
+---------+--------+------------+-------+
| WROOM-2 | N16R8V | OPI        | OPI   |
+---------+--------+------------+-------+
| WROOM-2 | N32R8V | OPI        | OPI   |
+---------+--------+------------+-------+


Further Help
------------

If you encounter any other issues or need further assistance, please consult the `ESP32 Arduino Core <https://github.com/espressif/arduino-esp32>`_ documentation or seek help from the `ESP32 community forums <https://esp32.com>`_.
