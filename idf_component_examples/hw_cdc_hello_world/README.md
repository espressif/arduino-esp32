| Supported Targets | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S3 |
| ----------------- | -------- | -------- | -------- | -------- |

# _HW Serial USB CDC example_

This is the simplest buildable example made to be used as a template for new projects running Arduino-ESP32 as an ESP-IDF component that will redefine the `Serial` interface to be attached to the USB CDC Hardware Serial port.\
See [arduino-esp32](https://components.espressif.com/components/espressif/arduino-esp32) in ESP Registry.

## How to use example

After cloning this repository, go to the `hw_cdc_hello_world` folder and select the target by executing\
`idf.py set-target <SoC_target>`.\
`<SoC_target>` can be one of the installed IDF version supported targets.

It is possible to just clone this folder be executing\
`idf.py create-project-from-example "espressif/arduino-esp32^3.0.5:hw_cdc_hello_world"`

For IDF 5.1.x and forward, the list of targets that support Hardware USB CDC are, at least: esp32s3, esp32c3, esp32c6 and esp32h2.\
Then just run command: `idf.py build` or `idf.py -p USB_PORT flash monitor`.

Usually, it is necessary to make the ESP32 SoC to enter in `Download Mode` before uploading the firmware.\
After that, just press `RESET/EN` to start the new firmware.

## Example folder contents

The project **hw_serial_example** contains one source file in C++ language [main.cpp](main/main.cpp). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project building configuration is contained in `CMakeLists.txt`
file that provide a set of directives and instructions describing the project's source files and targets
(executable, library, or both).

Below is the minimum list of files in the project folder.

```
├── CMakeLists.txt             Global project CMake configuration file
├── sdkconfig.defaults         sdkconfig setting for an Arduino project
├── main  
│   ├── CMakeLists.txt         Arduino sketch CMake configuration file
│   ├── idf_component.yml      List of IDF components necessary to build the project
│   └── main.cpp               Arduino Sketch code - don't forget to add "#include <Arduino.h>" on it
└── README.md                  This is the file you are currently reading
```

## Configuring the Hardware USB CDC Serial

ESP32 Arduino has two macro defined symbols that control what `Serial` symbol will represent.
Default `Serial` is the UART0 from `HardwareSerial` class.

`Serial` can be changed to be attached to the HW Serial JTAG port fro the SoC.
In order to make it work, it is necessary to define 2 symbols: `ARDUINO_USB_CDC_ON_BOOT` and `ARDUINO_USB_MODE` to `1`.
This is achieved by adding a couple lines to the [Project Folder CMakeLists.txt](CMakeLists.txt) file.


```
# Adds necessary definitions for compiling it using Serial symbol attached to the HW USB CDC port
list(APPEND compile_definitions "ARDUINO_USB_CDC_ON_BOOT=1")
list(APPEND compile_definitions "ARDUINO_USB_MODE=1")

```

Those two lines will add a `-DSYMBOL=VAL` when compiling every source code file.

In order to make sure that it is actually working correctly, the [sketch](main/main.cpp) will execute `Serial.begin();` with no baudrate, which only works for USB CDC.
