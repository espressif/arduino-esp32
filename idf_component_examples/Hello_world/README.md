| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- |

# _Hello world example_

This is the simplest buildable example made to be used as a template for new projects running Arduino-esp32 as an ESP-IDF component.
See [Arduino-esp32](https://components.espressif.com/components/espressif/arduino-esp32) in ESP Registry.

## How to use example

To create a ESP-IDF project from this example with the latest relase of Arduino-esp32, you can simply run command: `idf.py create-project-from-example "espressif/arduino-esp32:hello_world"`.
ESP-IDF will download all dependencies needed from the component registry and setup the project for you.

If you want to use cloned Arduino-esp32 repository, you can build this example directly.
Go to the example folder `arduino-esp32/idf_component_examples/Hello_world`.
First you need to comment line 6 `pre_release: true` in examples `/main/idf_component.yml`.
Then just run command: `idf.py build`.

## Example folder contents

The project **Hello_world** contains one source file in C++ language [main.cpp](main/main.cpp). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp
└── README.md                  This is the file you are currently reading
```

## How to add Arduino libraries

In the project create folder `components/` and clone the library there.
In the library folder create new CMakeLists.txt file, add lines shown below to the file and edit the SRCS to match the library source files.

```
idf_component_register(SRCS "user_library.cpp" "another_source.c"
                      INCLUDE_DIRS "."
                      REQUIRES arduino-esp32
                      )
```

Below is structure of the project folder with the Arduino libraries.

```
├── CMakeLists.txt
├── components
│   ├── user_library
│   │   ├── CMakeLists.txt     This needs to be added
│   │   ├── ...
├── main
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp
└── README.md                  This is the file you are currently reading
```