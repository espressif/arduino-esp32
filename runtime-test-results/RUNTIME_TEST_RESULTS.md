## Runtime Test Results

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C5|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|2/2 :white_check_mark:|-|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|1/1 :white_check_mark:|1/1 :white_check_mark:
fs|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|0/1 :x:|51/51 :white_check_mark:|51/51 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|0/1 :x:|1/1 :white_check_mark:|1/1 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|1/1 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|0/4 :x:|2/2 :white_check_mark:|3/3 :white_check_mark:
periman|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|0/1 :x:|1/1 :white_check_mark:|1/1 :white_check_mark:
psram|10/10 :white_check_mark:|-|10/10 :white_check_mark:|-|-|0/1 :x:|10/10 :white_check_mark:|10/10 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|0/1 :x:|4/4 :white_check_mark:|4/4 :white_check_mark:
touch|3/3 :white_check_mark:|-|-|-|-|0/1 :x:|3/3 :white_check_mark:|3/3 :white_check_mark:
uart|11/11 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|0/1 :x:|11/11 :white_check_mark:|10/10 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|0/1 :x:|2/2 :white_check_mark:|2/2 :white_check_mark:

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
uart|11/11 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:
wifi|2/2 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|2/2 :white_check_mark:|3/3 :white_check_mark:


Generated on: 2025/12/19 01:27:46

[Commit](https://github.com/espressif/arduino-esp32/commit/77ab98fc74adff3f69eee37cf7dcb90d219c053e) / [Build and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/20355345267) / [Hardware and Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/20355499664) / [Results processing](https://github.com/espressif/arduino-esp32/actions/runs/20356742415)

[Test results](https://github.com/espressif/arduino-esp32/runs/58493724374)
