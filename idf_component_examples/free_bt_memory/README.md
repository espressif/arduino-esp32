| Supported Targets | All with BT/BLE support |
| ----------------- | ----------------------- |

# _free_bt_memory example_

Shows how to override the weak `btInUse()` definition when using Arduino-esp32 as an ESP-IDF component so the Bluetooth controller memory is released when Bluetooth is not needed. Return `true` from `btInUse()` only if your app will use BT/BLE.

This is useful when bluetooth use is decided at runtime. For example, if you want to use bluetooth only when a pin is `HIGH`, you can drive the pin `HIGH` to use bluetooth and drive it `LOW` to free the bluetooth memory.

## How to use example

Create a project from the registry example:

`idf.py create-project-from-example "espressif/arduino-esp32:free_bt_memory"`

Or build directly from a cloned Arduino-esp32 checkout:

```
cd arduino-esp32/idf_component_examples/free_bt_memory
idf.py build
```
