## Runtime Test Results

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C5|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:
ble|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:
democfg|Error :fire:|-|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
fs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hash|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hello_world|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
keyboard_layout|-|-|-|-|-|Error :fire:|Error :fire:|Error :fire:
nvs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
periman|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
psram|Error :fire:|-|Error :fire:|-|-|Error :fire:|Error :fire:|Error :fire:
signed_ota|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|Error :fire:|Error :fire:|Error :fire:
timer|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
touch|Error :fire:|-|-|-|-|Error :fire:|Error :fire:|Error :fire:
uart|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
unity|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
webserver|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
wifi|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|Error :fire:|Error :fire:|Error :fire:
wifi_ap|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:

#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
console|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:
fs|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:
gpio|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
hash|72/72 :white_check_mark:|72/72 :white_check_mark:|72/72 :white_check_mark:|72/72 :white_check_mark:|72/72 :white_check_mark:|72/72 :white_check_mark:|72/72 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
i2c_master|7/7 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:|6/6 :white_check_mark:|6/6 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:
keyboard_layout|-|-|-|-|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:
psram|10/10 :white_check_mark:|-|-|-|8/8 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
sdcard|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:
uart|15/16 :x:|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:|16/16 :white_check_mark:|15/15 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:
wifi|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:

### Performance Tests

- **coremark**
  - ESP32 - Error - :fire:
  - ESP32-C3 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-C6 - Error - :fire:
  - ESP32-H2 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:

- **fibonacci**
  - ESP32 - Error - :fire:
  - ESP32-C3 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-C6 - Error - :fire:
  - ESP32-H2 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:

- **linpack_double**
  - ESP32 - Error - :fire:
  - ESP32-C3 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-C6 - Error - :fire:
  - ESP32-H2 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:

- **linpack_float**
  - ESP32 - Error - :fire:
  - ESP32-C3 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-C6 - Error - :fire:
  - ESP32-H2 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:

- **psramspeed**
  - ESP32 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:

- **ramspeed**
  - ESP32 - Error - :fire:
  - ESP32-C3 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-C6 - Error - :fire:
  - ESP32-H2 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:

- **superpi**
  - ESP32 - Error - :fire:
  - ESP32-C3 - Error - :fire:
  - ESP32-C5 - Error - :fire:
  - ESP32-C6 - Error - :fire:
  - ESP32-H2 - Error - :fire:
  - ESP32-P4 - Error - :fire:
  - ESP32-S2 - Error - :fire:
  - ESP32-S3 - Error - :fire:



Generated on: 2026/04/29 06:49:59

[Commit](https://github.com/espressif/arduino-esp32/commit/332a9acffd7bc57f6d454985dc4ea4fb5d2a68c2) / [Build and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/25084772766) / [Hardware and Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/25085184493) / [Results processing](https://github.com/espressif/arduino-esp32/actions/runs/25094913461)

[Test results](https://github.com/espressif/arduino-esp32/runs/73529435763)
