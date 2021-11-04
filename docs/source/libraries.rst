#########
Libraries
#########

Here is where the Libraries API's descriptions are located.

Supported Peripherals
---------------------

Currently, the Arduino ESP32 supports the following peripherals with Arduino style. Some other peripherals are not supported yet, but it's supported using ESP-IDF style.

+-------------+-------------+---------+--------------------+
| Peripheral  | Arduino API | ESP-IDF | Comment            |
+=============+=============+=========+====================+
| ADC         | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| DAC         | No          | No      |                    |
+-------------+-------------+---------+--------------------+
| GPIO        | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| SDIO/SPI    | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| I2C         | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| I2S         | No          | Yes     | In Development     |
+-------------+-------------+---------+--------------------+
| Wi-Fi       | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| Bluetooth   | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| RMT         | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| Touch       | Yes         | Yes     | ESP32 & ESP32-S2   |
+-------------+-------------+---------+--------------------+
| Timer       | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| UART        | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| Hall Sensor | Yes         | Yes     | Only for ESP32     |
+-------------+-------------+---------+--------------------+
| LEDC        | Yes         | Yes     |                    |
+-------------+-------------+---------+--------------------+
| Motor PWM   | No          | No      |                    |
+-------------+-------------+---------+--------------------+
| TWAI        | No          | No      |                    |
+-------------+-------------+---------+--------------------+
| Ethernet    | Yes         | Yes     | Only for ESP32     |
+-------------+-------------+---------+--------------------+
| USB         | Yes         | Yes     | Only for ESP32-S2  |
+-------------+-------------+---------+--------------------+

Some peripherals are not available for all ESP32 families. To see more details about it, see the corresponding datasheet.

APIs
----

The Arduino ESP32 offers some unique APIs, described in this section:

.. toctree::
    :maxdepth: 1

    Bluetooth <api/bluetooth>
    Deep Sleep <api/deepsleep>
    ESPNOW <api/espnow>
    GPIO <api/gpio>
    RainMaker <api/rainmaker>
    Reset Reason <api/reset_reason>
    Wi-Fi <api/wifi>
