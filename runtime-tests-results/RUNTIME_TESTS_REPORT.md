## Runtime Tests Report

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:
hello_world|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:
nvs|Error :fire:|2/2 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|1/1 :white_check_mark:|2/2 :white_check_mark:|Error :fire:
periman|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|Error :fire:
psram|Error :fire:|-|-|-|8/8 :white_check_mark:|10/10 :white_check_mark:|Error :fire:
timer|Error :fire:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|Error :fire:
touch|Error :fire:|-|-|-|3/3 :white_check_mark:|3/3 :white_check_mark:|Error :fire:
uart|Error :fire:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|9/10 :x:|10/11 :x:|Error :fire:
unity|Error :fire:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|Error :fire:
#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
gpio|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
i2c_master|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|1/1 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:
psram|10/10 :white_check_mark:|-|-|-|8/8 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:
uart|11/11 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:
wifi|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|2/2 :white_check_mark:|3/3 :white_check_mark:


Generated on: 2025/02/26 03:24:49

[Build, Hardware and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/13535165114) / [Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/13535829179)
