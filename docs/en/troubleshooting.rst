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

To be able to upload the sketch via serial interface, the ESP32 must be in the download mode. The download mode allows you to upload the sketch over the serial port and to get into it, you need to keep the **GPIO0** in LOW while a resetting (**EN** pin) cycle.
If you are trying to upload a new sketch and your board is not responding, there are some possible reasons.

Possible fatal error message from the Arduino IDE:

    *A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header*

Solution
^^^^^^^^

Here are some steps that you can try to:

* Check your USB cable and try a new one.
* Change the USB port.
* Check your power supply.
* In some instances, you must keep **GPIO0** LOW during the uploading process via the serial interface.
* Hold down the **“BOOT”** button in your ESP32 board while uploading/flashing.

In some development boards, you can try adding the reset delay circuit, as described in the *Power-on Sequence* section on the `ESP32 Hardware Design Guidelines <https://www.espressif.com/sites/default/files/documentation/esp32_hardware_design_guidelines_en.pdf>`_ in order to get into the download mode automatically.

Hardware
--------

Why is my computer not detecting my board?
**************************************************

If your board is not being detected after connecting to the USB, you can try to:

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

Please note that WEP/WPA has significant security vulnerabilities and its use is strongly discouraged.
The support may therefore be removed in the future. Please migrate to WPA2 or newer.

Solution
^^^^^^^^

Nevertheless, it may be necessary to connect to insecure networks. To do this, the security requirement of the ESP32 must be lowered to an insecure level by using:

.. code-block:: arduino

    WiFi.setMinSecurity(WIFI_AUTH_WEP); // Lower min security to WEP.
    // or
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK); // Lower min security to WPA.

Why does the board not connect to WPA3-encrypted Wi-Fi?
*******************************************************

WPA3 support is resource intensive and may not be compiled into the used SDK.

Solution
^^^^^^^^

* Check WPA3 support by your SDK.
* Compile your custom SDK with WPA3 support.

Sample code to check SDK WPA3 support at compile time:

.. code-block:: arduino

    #ifndef CONFIG_ESP32_WIFI_ENABLE_WPA3_SAE
    #warning "No WPA3 support."
    #endif
