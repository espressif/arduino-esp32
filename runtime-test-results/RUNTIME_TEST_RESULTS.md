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
ethernet|Error :fire:|-|-|-|-|Error :fire:|-|-
fs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hash|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hello_world|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
i2s|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
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
console|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:
eeprom|31/31 :white_check_mark:|31/31 :white_check_mark:|31/31 :white_check_mark:|31/31 :white_check_mark:|31/31 :white_check_mark:|31/31 :white_check_mark:|31/31 :white_check_mark:
fs|61/61 :white_check_mark:|61/61 :white_check_mark:|61/61 :white_check_mark:|61/61 :white_check_mark:|61/61 :white_check_mark:|61/61 :white_check_mark:|61/61 :white_check_mark:
gpio|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
hash|107/107 :white_check_mark:|107/107 :white_check_mark:|107/107 :white_check_mark:|107/107 :white_check_mark:|107/107 :white_check_mark:|107/107 :white_check_mark:|107/107 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
i2c_master|8/8 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:
keyboard_layout|-|-|-|-|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
multitasking|10/10 :white_check_mark:|9/9 :white_check_mark:|9/9 :white_check_mark:|9/9 :white_check_mark:|10/10 :white_check_mark:|9/9 :white_check_mark:|10/10 :white_check_mark:
network_client|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:
networking|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:|-|13/13 :white_check_mark:|13/13 :white_check_mark:|13/13 :white_check_mark:
nvs|52/52 :white_check_mark:|52/52 :white_check_mark:|104/104 :white_check_mark:|104/104 :white_check_mark:|104/104 :white_check_mark:|52/52 :white_check_mark:|78/78 :white_check_mark:
psram|14/14 :white_check_mark:|-|-|-|11/11 :white_check_mark:|14/14 :white_check_mark:|14/14 :white_check_mark:
sdcard|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:
spi|16/16 :white_check_mark:|16/16 :white_check_mark:|16/16 :white_check_mark:|16/16 :white_check_mark:|16/16 :white_check_mark:|16/16 :white_check_mark:|16/16 :white_check_mark:
ticker|19/19 :white_check_mark:|19/19 :white_check_mark:|19/19 :white_check_mark:|19/19 :white_check_mark:|19/19 :white_check_mark:|19/19 :white_check_mark:|19/19 :white_check_mark:
timer|7/7 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:|8/8 :white_check_mark:
uart|26/26 :white_check_mark:|25/25 :white_check_mark:|25/25 :white_check_mark:|25/25 :white_check_mark:|25/25 :white_check_mark:|26/26 :white_check_mark:|25/25 :white_check_mark:
unity|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:|15/15 :white_check_mark:
wifi|34/34 :white_check_mark:|17/17 :white_check_mark:|17/17 :white_check_mark:|-|-|34/34 :white_check_mark:|51/51 :white_check_mark:

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



Generated on: 2026/10/03 00:33:32 UTC

[Commit](https://github.com/espressif/arduino-esp32/commit/aeabde6e33e4dfe44842166975032b68f6e479a7) / [Build and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/37081267210) / [Hardware and Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/37081952922) / [Results processing](https://github.com/espressif/arduino-esp32/actions/runs/37082441819)

[Test results](https://github.com/espressif/arduino-esp32/runs/111086058392)
