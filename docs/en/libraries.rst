#########
Libraries
#########

Here is where the Libraries API's descriptions are located:

Supported Peripherals
---------------------

Currently, the Arduino ESP32 supports the following peripherals with Arduino APIs.

+---------------+-------+-------+-------+-------+-------+-------+-------+
| Peripheral    | ESP32 | S2    | C3    | S3    | C6    | H2    | Notes |
+===============+=======+=======+=======+=======+=======+=======+=======+
| ADC           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| BT Classic    | Yes   | N/A   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| BLE           | Yes   | N/A   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| DAC           | Yes   | Yes   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Ethernet      | Yes   | N/A   | N/A   | N/A   | N/A   | N/A   | (*)   |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| GPIO          | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Hall Sensor   | N/A   | N/A   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| I2C           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| I2S           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| LEDC          | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Motor PWM     | No    | N/A   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Pulse Counter | No    | No    | No    | No    | No    | No    |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| RMT           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| SDIO          | No    | No    | No    | No    | No    | No    |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| SDMMC         | Yes   | N/A   | N/A   | Yes   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Timer         | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Temp. Sensor  | N/A   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Touch         | Yes   | Yes   | N/A   | Yes   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| TWAI          | No    | No    | No    | No    | No    | No    |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| UART          | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| USB           | N/A   | Yes   | Yes   | Yes   | Yes   | Yes   | (**)  |
+---------------+-------+-------+-------+-------+-------+-------+-------+
| Wi-Fi         | Yes   | Yes   | Yes   | Yes   | Yes   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+

Notes
^^^^^

(*) SPI Ethernet is supported by all ESP32 families and RMII only for ESP32.

(**) ESP32-C3, C6, H2 only support USB CDC/JTAG

.. note:: Some peripherals are not available for all ESP32 families. To see more details about it, see the corresponding SoC at `Product Selector <https://products.espressif.com>`_ page.

.. include:: common/datasheet.inc

APIs
----

The Arduino ESP32 offers some unique APIs, described in this section:

.. note::
    Please be advised that we cannot ensure continuous compatibility between the Arduino Core ESP32 APIs and ESP8266 APIs, as well as Arduino-Core APIs (Arduino.cc).
    While our aim is to maintain harmony, the addition of new features may result in occasional divergence. We strive to achieve the best possible integration but acknowledge
    that perfect compatibility might not always be feasible. Please refer to the documentation for any specific considerations.

.. toctree::
    :maxdepth: 1
    :glob:

    api/*
