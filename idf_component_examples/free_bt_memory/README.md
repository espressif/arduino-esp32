| Supported Targets | ESP32 | ESP32-S3 | ESP32-C3 |
| ----------------- | ----- | -------- | -------- |

# _free_bt_memory example_

Shows how to override the weak `btInUse()` definition when using Arduino-esp32 as an ESP-IDF component so the Bluetooth controller memory is released when Bluetooth is not needed. Return `true` from `btInUse()` only if your app will use BT/BLE.

## How to use example

Create a project from the registry example:

`idf.py create-project-from-example "espressif/arduino-esp32:free_bt_memory"`

Or build directly from a cloned Arduino-esp32 checkout:

```
cd arduino-esp32/idf_component_examples/free_bt_memory
idf.py build
```

## Example folder contents

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp
└── README.md
```
