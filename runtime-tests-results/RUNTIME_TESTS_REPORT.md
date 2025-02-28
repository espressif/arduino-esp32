## Runtime Tests Report

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|Error :fire:|-|1/1 :white_check_mark:|-|-|Error :fire:|Error :fire:
hello_world|Error :fire:|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|Error :fire:
nvs|Error :fire:|Error :fire:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|Error :fire:|Error :fire:
periman|Error :fire:|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|Error :fire:|Error :fire:
psram|Error :fire:|-|-|-|8/8 :white_check_mark:|Error :fire:|Error :fire:
timer|Error :fire:|Error :fire:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|Error :fire:|Error :fire:
touch|Error :fire:|-|-|-|3/3 :white_check_mark:|Error :fire:|Error :fire:
uart|Error :fire:|Error :fire:|10/10 :white_check_mark:|10/10 :white_check_mark:|9/10 :x:|Error :fire:|Error :fire:
unity|Error :fire:|Error :fire:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|Error :fire:|Error :fire:
#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
gpio|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:
hello_world|0/1 :x:|1/1 :white_check_mark:|1/1 :white_check_mark:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:
i2c_master|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:
nvs|0/2 :x:|0/2 :x:|0/4 :x:|0/4 :x:|0/4 :x:|0/2 :x:|0/3 :x:
psram|0/1 :x:|-|-|-|0/1 :x:|0/1 :x:|0/1 :x:
timer|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:
uart|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:
unity|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:|0/1 :x:
wifi|0/2 :x:|0/1 :x:|0/1 :x:|-|-|0/2 :x:|0/3 :x:


Generated on: 2025/02/28 03:56:48

[Build, Hardware and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/13580002473) / [Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/13580570732)
