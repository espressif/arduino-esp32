## Runtime Tests Report

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|2/2 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|Error :fire:|Error :fire:
hello_world|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|Error :fire:|Error :fire:
nvs|2/2 :white_check_mark:|Error :fire:|4/4 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|Error :fire:|Error :fire:
periman|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|Error :fire:|-|Error :fire:|Error :fire:
psram|9/9 :white_check_mark:|-|-|-|9/9 :white_check_mark:|Error :fire:|Error :fire:
timer|3/3 :white_check_mark:|Error :fire:|4/4 :white_check_mark:|Error :fire:|4/4 :white_check_mark:|Error :fire:|Error :fire:
touch|3/3 :white_check_mark:|-|-|-|3/3 :white_check_mark:|Error :fire:|Error :fire:
uart|9/11 :x:|Error :fire:|10/10 :white_check_mark:|Error :fire:|9/10 :x:|Error :fire:|Error :fire:
unity|2/2 :white_check_mark:|Error :fire:|2/2 :white_check_mark:|Error :fire:|2/2 :white_check_mark:|Error :fire:|Error :fire:
#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
gpio|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
i2c_master|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:|5/5 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|1/1 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:
psram|9/9 :white_check_mark:|-|-|-|0/1 :x:|9/9 :white_check_mark:|9/9 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:
uart|11/11 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:
wifi|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|2/2 :white_check_mark:|3/3 :white_check_mark:


Generated on: 2025/02/22 03:32:25

[Build, Hardware and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/13468621914) / [Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/13469154323)
