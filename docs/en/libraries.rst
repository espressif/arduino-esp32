#########
Libraries
#########

Arduino libraries help you use the features of the ESP32 family of chips with the familiar Arduino API.

Supported Features and Peripherals
----------------------------------

Currently, the Arduino ESP32 supports almost everything available on the ESP32 family with an Arduino-like API.

Not all features are available on all SoCs. Please check the `Product Selector <https://products.espressif.com>`_ page
for more details.

Here is a matrix of the library support status for the main features and peripherals per SoC:

- |yes| Supported through the Arduino Core
- |no| Not supported through the Arduino Core. It can still be used through the ESP-IDF API, but might require rebuilding the static libraries.
- |n/a| Not available on the SoC

.. rst-class:: table-wrap

.. Using substitions rather than emojis directly because in macOS vscode the emojis don't take a fixed space in the text
   and the table looks weird and hard to edit. This is a workaround to make the table easier to edit. Just write
   |yes|, |no|, |n/a| instead of emojis.

+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Feature              | ESP32 | C2    | C3    | C5    | C6    | C61   | H2    | P4    | S2    | S3    |
+======================+=======+=======+=======+=======+=======+=======+=======+=======+=======+=======+
| ADC [1]_             | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| BT Classic           | |yes| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| BLE [2]_             | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |n/a| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| DAC                  | |yes| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |yes| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| ESP-NOW [3]_         | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |n/a| | |n/a| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Ethernet [4]_        | |yes| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |yes| | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| GPIO                 | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Hall Sensor          | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| I2C                  | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| I2S                  | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| I3C                  | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |no|  | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| LEDC                 | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Matter (Thread) [6]_ | |n/a| | |n/a| | |n/a| | |yes| | |yes| | |n/a| | |yes| | |n/a| | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Matter (Wi-Fi)       | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |n/a| | |n/a| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| MIPI CSI             | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |no|  | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| MIPI DSI             | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |no|  | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Motor PWM            | |no|  | |n/a| | |n/a| | |no|  | |no|  | |n/a| | |no|  | |no|  | |n/a| | |no|  |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| MSPI                 | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |no|  | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Pulse Counter        | |no|  | |no|  | |no|  | |no|  | |no|  | |no|  | |no|  | |no|  | |no|  | |no|  |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| RMT                  | |yes| | |yes| | |yes| | |yes| | |yes| | |n/a| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| SDIO                 | |no|  | |n/a| | |n/a| | |no|  | |no|  | |no|  | |n/a| | |no|  | |n/a| | |no|  |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| SDMMC                | |yes| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| PSRAM                | |yes| | |n/a| | |n/a| | |yes| | |n/a| | |yes| | |n/a| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Temp. Sensor         | |n/a| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Thread               | |n/a| | |n/a| | |n/a| | |yes| | |yes| | |n/a| | |yes| | |n/a| | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Timer                | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Touch                | |yes| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| TWAI/CAN-FD          | |no|  | |n/a| | |no|  | |no|  | |no|  | |n/a| | |no|  | |no|  | |no|  | |no|  |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| UART                 | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| USB OTG              | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |n/a| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| USB Serial           | |n/a| | |n/a| | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |n/a| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Wi-Fi [2]_           | |yes| | |yes| | |yes| | |yes| | |yes| | |yes| | |n/a| | |yes| | |yes| | |yes| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
| Zigbee [5]_          | |n/a| | |n/a| | |n/a| | |yes| | |yes| | |n/a| | |yes| | |n/a| | |n/a| | |n/a| |
+----------------------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+

.. [1] ESP32-P4 calibration schemes not supported yet in IDF and ADC Continuous also lacks IDF support.

.. [2] ESP32-P4 only supports Wi-Fi and BLE through another SoC by using ``ESP-Hosted``.

.. [3] ESP-NOW is not supported through ``ESP-Hosted``.

.. [4] SPI Ethernet is supported by all ESP32 families and RMII only for ESP32 and ESP32-P4.

.. [5] Non-native Zigbee SoCs can also run Zigbee, but must use another SOC (with Zigbee radio) as a RCP connected by UART/SPI.
   Check the `Gateway example <https://github.com/espressif/arduino-esp32/tree/master/libraries/Zigbee/examples/Zigbee_Gateway>`_ for more details.

.. [6] Matter over Thread is supported by our library but is not included in the pre-compiled libraries.
   In order to use Matter over Thread, you need to use Arduino as an ESP-IDF component or rebuild the static libraries.
   Check the `Arduino_ESP_Matter_over_OpenThread example <https://github.com/espressif/arduino-esp32/tree/master/idf_component_examples/Arduino_ESP_Matter_over_OpenThread>`_ for more details.

.. note:: The ESP32-C2 and ESP32-C61 are only supported using Arduino as an ESP-IDF component or by rebuilding the static libraries.

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

Matter APIs
-----------

.. toctree::
    :maxdepth: 1
    :glob:

    matter/matter
    matter/matter_ep
    matter/ep_*

OpenThread APIs
---------------

.. toctree::
    :maxdepth: 1
    :glob:

    openthread/openthread
    openthread/openthread_cli
    openthread/openthread_core
    openthread/openthread_dataset

Zigbee APIs
-----------

.. toctree::
    :maxdepth: 1
    :glob:

    zigbee/zigbee
    zigbee/zigbee_core
    zigbee/zigbee_ep
    zigbee/ep_*
