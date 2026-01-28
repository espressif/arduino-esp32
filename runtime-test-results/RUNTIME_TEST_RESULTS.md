## Runtime Test Results

:x: **The test workflows are failing. Please check the run logs.** :x:

### Validation Tests

#### Hardware

Test|ESP32|ESP32-C3|ESP32-C5|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:
democfg|2/2 :white_check_mark:|-|1/1 :white_check_mark:|1/1 :white_check_mark:|-|-|1/1 :white_check_mark:|1/1 :white_check_mark:
fs|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:|51/51 :white_check_mark:
hello_world|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
nvs|2/2 :white_check_mark:|2/2 :white_check_mark:|1/1 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|2/2 :white_check_mark:|3/3 :white_check_mark:
periman|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:|1/1 :white_check_mark:
psram|10/10 :white_check_mark:|-|Error :fire:|-|-|8/8 :white_check_mark:|10/10 :white_check_mark:|10/10 :white_check_mark:
timer|3/3 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:|4/4 :white_check_mark:
touch|3/3 :white_check_mark:|-|-|-|-|3/3 :white_check_mark:|3/3 :white_check_mark:|3/3 :white_check_mark:
uart|12/12 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|11/11 :white_check_mark:|12/12 :white_check_mark:|11/11 :white_check_mark:
unity|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:|2/2 :white_check_mark:

#### Wokwi

Test|ESP32|ESP32-C3|ESP32-C6|ESP32-H2|ESP32-P4|ESP32-S2|ESP32-S3
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
fs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
gpio|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
hello_world|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
i2c_master|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
nvs|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
psram|Error :fire:|-|-|-|Error :fire:|Error :fire:|Error :fire:
sdcard|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
timer|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
uart|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
unity|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:|Error :fire:
wifi|Error :fire:|Error :fire:|Error :fire:|-|-|Error :fire:|Error :fire:


Generated on: 2026/01/28 01:57:23

[Commit](https://github.com/espressif/arduino-esp32/commit/0ed36b09f4ed307b0f0850e38aa2f6f4104a39f6) / [Build and QEMU run](https://github.com/espressif/arduino-esp32/actions/runs/21419412016) / [Hardware and Wokwi run](https://github.com/espressif/arduino-esp32/actions/runs/21420342742) / [Results processing](https://github.com/espressif/arduino-esp32/actions/runs/21421390494)

[Test results](https://github.com/espressif/arduino-esp32/runs/61682742069)
