## Runtime Test Results

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C5|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:
ble|0/1 :x:|0/1 :x:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|Error :fire:
democfg|2/2 :white_check_mark:|-|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|1/1 :white_check_mark:|Error :fire:
fs|51/51 :white_check_mark:|Error :fire:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|Error :fire:|51/51 :white_check_mark:|Error :fire:
hello_world|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|Error :fire:
nvs|2/2 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|Error :fire:|2/2 :white_check_mark:|Error :fire:
periman|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|Error :fire:|1/1 :white_check_mark:|Error :fire:
psram|Error :fire:|-|10/10 :white_check_mark:|-|-|Error :fire:|10/10 :white_check_mark:|10/10 :white_check_mark:
timer|3/3 :white_check_mark:|Error :fire:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|Error :fire:|4/4 :white_check_mark:|Error :fire:
touch|3/3 :white_check_mark:|-|-|-|-|Error :fire:|3/3 :white_check_mark:|Error :fire:
uart|12/12 :white_check_mark:|Error :fire:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|Error :fire:|12/12 :white_check_mark:|Error :fire:
unity|2/2 :white_check_mark:|Error :fire:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|Error :fire:|2/2 :white_check_mark:|Error :fire:
wifi_ap|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|2/2 :white_check_mark:|Error :fire:

#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
fs|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:
gpio|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
i2c_master|7/7 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:|6/6 :white_check_mark:|6/6 :white_check_mark:|7/7 :white_check_mark:|7/7 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:
psram|10/10 :white_check_mark:|-|-|-|8/8 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
sdcard|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:
uart|12/12 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|12/12 :white_check_mark:|11/11 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:
wifi|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|1/1 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:


Generated on: 2026/02/17 01:11:10

[Commit](https://github.com/espressif/arduino-esp32/commit/37ef13003020c8f30a00ef9366572fe6d7c5417c) / [Build and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/22081427150) / [Hardware and Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/22081644484) / [Results processing](https://github.com/espressif/arduino-esp32/actions/runs/22082438911)

[Test results](https://github.com/espressif/arduino-esp32/runs/63810297597)
