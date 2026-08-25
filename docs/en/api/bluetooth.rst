#########
Bluetooth
#########

About
-----

.. note:: This is a work in progress project and this section is still missing. If you want to contribute, please see the `Contributions Guide <../contributing.html>`_.

The BluetoothSerial library provides Bluetooth Classic support on SoCs that include it.

If you use the ``BluetoothSerial`` library, the core keeps the Bluetooth memory regions
reserved automatically.

Using ESP-IDF Bluetooth APIs directly
-------------------------------------

If your sketch calls ESP-IDF Bluetooth APIs directly instead of using Arduino libraries,
include the matching header in at least one source file:

* **BLE:** ``esp32-hal-alloc-ble-mem.h``
* **Bluetooth Classic:** ``esp32-hal-alloc-bt-classic-mem.h``

For example:

.. code-block:: arduino

    #include "esp32-hal-alloc-bt-classic-mem.h"

This tells the core that Bluetooth is in use. During ``initArduino()``, the core will not
release the corresponding controller and host memory back to the heap. Without the
correct include, Bluetooth initialization can fail or behave unpredictably.

Examples
--------

To get started with Bluetooth, you can try:

Serial To Serial BT
*******************

.. literalinclude:: ../../../libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino
    :language: arduino

BT Classic Device Discovery
***************************

.. literalinclude:: ../../../libraries/BluetoothSerial/examples/bt_classic_device_discovery/bt_classic_device_discovery.ino
    :language: arduino

Complete list of `Bluetooth examples <https://github.com/espressif/arduino-esp32/tree/master/libraries/BluetoothSerial/examples>`_.
