###
BLE
###

About
-----

.. note:: This is a work in progress project and this section is still missing. If you want to contribute, please see the `Contributions Guide <../contributing.html>`_.

The BLE library wraps the ESP-IDF host stack with an Arduino-style API.

If you use the ``BLE`` library (for example ``BLEDevice::init()``), the core keeps the
Bluetooth memory regions reserved automatically.

.. warning::

   Do **not** use this library in a Matter sketch. When CHIPoBLE is compiled in,
   Matter owns the BLE host (NimBLE if that stack is selected). ``BLEDevice::init()``
   will fail or crash after ``Matter.begin()``, including when
   ``Matter.setBLECommissioningEnabled(false)`` releases BLE RAM, and after
   CHIPoBLE commissioning when ``Matter.setBLEMemoryReleaseEnabled(true)`` (the default).
   Use ``Matter.onBLEMemoryReleased()`` to run code after that RAM is back on the heap.
   See :doc:`../matter/matter`.

Using ESP-IDF BLE APIs directly
---------------------------------

If your sketch calls ESP-IDF BLE APIs directly (for example ``nimble_port_init()`` or
``esp_ble_mesh_init()``), include ``esp32-hal-alloc-ble-mem.h`` in at least one source
file:

.. code-block:: arduino

    #include "esp32-hal-alloc-ble-mem.h"

This tells the core that BLE is in use. During ``initArduino()``, the core will not
release BLE controller and host memory back to the heap. Without this include, BLE
initialization can fail or behave unpredictably.

Examples
--------

To get started with BLE, you can try:

BLE Scan
********

.. literalinclude:: ../../../libraries/BLE/examples/Scan/Scan.ino
    :language: arduino

BLE UART
********

.. literalinclude:: ../../../libraries/BLE/examples/UART/UART.ino
    :language: arduino

Complete list of `BLE examples <https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE/examples>`_.
