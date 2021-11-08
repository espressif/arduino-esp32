#########
Libraries
#########

Here is where the Libraries API's descriptions are located:

Supported Peripherals
---------------------

Currently, the Arduino ESP32 supports the following peripherals with Arduino APIs.

+---------------+---------------+---------------+---------------+-------------------------------+
| Peripheral    | ESP32         | ESP32-S2      | ESP32-C3      | Comments                      |
+===============+===============+===============+===============+===============================+
| ADC           | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Bluetooth     | Yes           | Not Supported | Not Supported | Bluetooth Classic             |
+---------------+---------------+---------------+---------------+-------------------------------+
| BLE           | Yes           | Not Supported | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| DAC           | Yes           | Yes           | Not Supported |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Ethernet      | Yes           | Not Supported | Not Supported | (*)                           |
+---------------+---------------+---------------+---------------+-------------------------------+
| GPIO          | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Hall Sensor   | Yes           | Not Supported | Not Supported |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| I2C           | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| I2S           | No            | No            | No            | WIP                           |
+---------------+---------------+---------------+---------------+-------------------------------+
| LEDC          | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Motor PWM     | No            | Not Supported | Not Supported |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Pulse Counter | No            | No            | No            |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| RMT           | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| SDIO          | No            | No            | No            |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| SPI           | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Timer         | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Temp. Sensor  | Not Supported | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| Touch         | Yes           | Yes           | Not Supported |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| TWAI          | No            | No            | No            |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| UART          | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+
| USB           | Not Supported | Yes           | Yes           | ESP32-C3 only CDC/JTAG        |
+---------------+---------------+---------------+---------------+-------------------------------+
| Wi-Fi         | Yes           | Yes           | Yes           |                               |
+---------------+---------------+---------------+---------------+-------------------------------+

Notes
^^^^^

(*) SPI Ethernet is supported by all ESP32 families and RMII only for ESP32.

.. note:: Some peripherals are not available for all ESP32 families. To see more details about it, see the corresponding SoC at `Product Selector <https://products.espressif.com>`_ page.

Datasheet
^^^^^^^^^

* `ESP32 <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf>`_
* `ESP32-S2 <https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf>`_
* `ESP32-C3 <https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf>`_

APIs
----

The Arduino ESP32 offers some unique APIs, described in this section:

.. toctree::
    :maxdepth: 1

    Bluetooth <api/bluetooth>
    Deep Sleep <api/deepsleep>
    ESPNOW <api/espnow>
    GPIO <api/gpio>
    I2C <api/i2c>
    RainMaker <api/rainmaker>
    Reset Reason <api/reset_reason>
    Wi-Fi <api/wifi>
