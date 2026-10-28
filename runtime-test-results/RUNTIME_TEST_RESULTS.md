## Runtime Test Results

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C5|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:
adc_pwm|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
ble|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:
bt_classic|Error :fire:|-|-|-|-|-|-|-
bt_inuse_override|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:
bt_mem_wrap|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:
clock|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
dac|Error :fire:|-|-|-|-|-|Error :fire:|-
democfg|Error :fire:|-|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
eeprom|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
esp_now|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
fs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hash|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hello_world|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
i2s|Error :fire:|Error :fire:|-|Error :fire:|Error :fire:|-|Error :fire:|-
keyboard_layout|-|-|-|-|-|Error :fire:|Error :fire:|Error :fire:
multitasking|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
network_client|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
networking|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
nvs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
openthread|-|-|Error :fire:|Error :fire:|Error :fire:|-|-|-
ota|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
periman|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
power_management|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
psram|Error :fire:|-|Error :fire:|-|-|Error :fire:|Error :fire:|Error :fire:
signed_ota|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
ticker|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
timer|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
tls_http|Error :fire:|Error :fire:|Error :fire:|-|-|-|Error :fire:|Error :fire:
touch|Error :fire:|-|-|-|-|Error :fire:|Error :fire:|Error :fire:
uart|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
unity|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
webserver|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
wifi|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
wifi_ap|Error :fire:|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:
zigbee|-|-|Error :fire:|Error :fire:|Error :fire:|-|-|-

#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
console|Error :fire:|Error :fire:|17/17 :white_check_mark:|17/17 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
eeprom|Error :fire:|Error :fire:|31/31 :white_check_mark:|31/31 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
fs|Error :fire:|Error :fire:|61/61 :white_check_mark:|61/61 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
gpio|Error :fire:|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
hash|Error :fire:|Error :fire:|107/107 :white_check_mark:|107/107 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
hello_world|Error :fire:|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
i2c_master|Error :fire:|Error :fire:|8/8 :white_check_mark:|7/7 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
keyboard_layout|-|-|-|-|Error :fire:|Error :fire:|Error :fire:
multitasking|Error :fire:|Error :fire:|9/9 :white_check_mark:|9/9 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
network_client|Error :fire:|Error :fire:|13/13 :white_check_mark:|13/13 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
networking|Error :fire:|Error :fire:|13/13 :white_check_mark:|-|Error :fire:|Error :fire:|Error :fire:
nvs|Error :fire:|Error :fire:|104/104 :white_check_mark:|104/104 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
psram|Error :fire:|-|-|-|Error :fire:|Error :fire:|Error :fire:
sdcard|Error :fire:|Error :fire:|11/11 :white_check_mark:|11/11 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
spi|Error :fire:|Error :fire:|16/16 :white_check_mark:|16/16 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
ticker|Error :fire:|Error :fire:|19/19 :white_check_mark:|19/19 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
timer|Error :fire:|Error :fire:|11/11 :white_check_mark:|11/11 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
tls_http|Error :fire:|Error :fire:|-|-|-|Error :fire:|Error :fire:
uart|Error :fire:|Error :fire:|25/25 :white_check_mark:|25/25 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
unity|Error :fire:|Error :fire:|15/15 :white_check_mark:|15/15 :white_check_mark:|Error :fire:|Error :fire:|Error :fire:
wifi|Error :fire:|Error :fire:|17/17 :white_check_mark:|-|Error :fire:|Error :fire:|Error :fire:

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



Generated on: 2026/10/28 00:47:26 UTC

[Commit](https://github.com/espressif/arduino-esp32/commit/8e29d4de275cf58c767f453ff6ed26bea692dfec) / [Build and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/40607103055) / [Hardware and Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/40608068091) / [Results processing](https://github.com/espressif/arduino-esp32/actions/runs/40609400532)

[Test results](https://github.com/espressif/arduino-esp32/runs/122562180788)
