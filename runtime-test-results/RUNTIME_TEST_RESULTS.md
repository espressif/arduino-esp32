## Runtime Test Results

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|2/2 :white_check_mark:|-|Error :fire:|-|-|Error :fire:|1/1 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|Error :fire:|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|Error :fire:|Error :fire:|4/4 :white_check_mark:|Error :fire:|3/3 :white_check_mark:
periman|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|Error :fire:|-|Error :fire:|1/1 :white_check_mark:
psram|10/10 :white_check_mark:|-|-|-|8/8 :white_check_mark:|Error :fire:|10/10 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|Error :fire:|Error :fire:|4/4 :white_check_mark:|Error :fire:|4/4 :white_check_mark:
touch|3/3 :white_check_mark:|-|-|-|3/3 :white_check_mark:|Error :fire:|3/3 :white_check_mark:
uart|11/11 :white_check_mark:|10/10 :white_check_mark:|Error :fire:|Error :fire:|10/10 :white_check_mark:|Error :fire:|10/10 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|Error :fire:|Error :fire:|2/2 :white_check_mark:|Error :fire:|2/2 :white_check_mark:

#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
gpio|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
i2c_master|7/7 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:|6/6 :white_check_mark:|6/6 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:
psram|10/10 :white_check_mark:|-|-|-|8/8 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:
uart|11/11 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:
wifi|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|2/2 :white_check_mark:|3/3 :white_check_mark:


Generated on: 2025/09/08 00:52:58

[Commit](https://github.com/espressif/arduino-esp32/commit/7436eabe6e95b98c20c58f76e41cd3bb11734b76) / [Build, Hardware and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/17535799504) / [Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/17536359813)
