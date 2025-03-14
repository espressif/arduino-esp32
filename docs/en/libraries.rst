#########
Libraries
#########

Here is where the Libraries API's descriptions are located:

Supported Peripherals
---------------------

Currently, the Arduino ESP32 supports the following peripherals with Arduino APIs.

+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Peripheral    | ESP32 | C3    | C6    | H2    | P4    | S2    | S3    | Notes |
+===============+=======+=======+=======+=======+=======+=======+=======+=======+
| ADC           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |  (1)  |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| BT Classic    | Yes   | N/A   | N/A   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| BLE           | Yes   | Yes   | Yes   | Yes   | No    | N/A   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| DAC           | Yes   | N/A   | N/A   | N/A   | Yes   | Yes   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Ethernet      | Yes   | N/A   | N/A   | N/A   | Yes   | N/A   | N/A   |  (2)  |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| GPIO          | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Hall Sensor   | N/A   | N/A   | N/A   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| I2C           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| I2S           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| LEDC          | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| MIPI          | N/A   | N/A   | N/A   | N/A   | No    | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Motor PWM     | No    | N/A   | N/A   | N/A   | N/A   | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| MSPI          | N/A   | N/A   | N/A   | N/A   | No    | N/A   | N/A   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Pulse Counter | No    | No    | No    | No    | No    | No    | No    |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| RMT           | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| SDIO          | No    | No    | No    | No    | No    | No    | No    |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| SDMMC         | Yes   | N/A   | N/A   | N/A   | N/A   | N/A   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Timer         | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Temp. Sensor  | N/A   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Touch         | Yes   | N/A   | N/A   | N/A   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| TWAI          | No    | No    | No    | No    | No    | No    | No    |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| UART          | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |       |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| USB           | N/A   | Yes   | Yes   | Yes   | Yes   | Yes   | Yes   |  (3)  |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+
| Wi-Fi         | Yes   | Yes   | Yes   | N/A   | Yes   | Yes   | Yes   |  (4)  |
+---------------+-------+-------+-------+-------+-------+-------+-------+-------+

Notes
^^^^^

(1) ESP32-P4 calibration schemes not supported yet in IDF and ADC Continuous also lacks IDF support.

(2) SPI Ethernet is supported by all ESP32 families and RMII only for ESP32 and ESP32-P4.

(3) ESP32-C3, C6, H2 only support USB CDC/JTAG

(4) ESP32-P4 only supports Wi-Fi through another SoC by using ``esp_hosted``.

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
