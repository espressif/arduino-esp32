#######
USB API
#######

.. note:: This feature is only supported by ESP32-S2.

About
-----

The **Universal Serial Bus** is a widely used peripheral to exchange data between devices. USB was introduced on the ESP32-S2, supporting both device and host mode.

To learn about the USB, see the `USB.org`_ for developers.

USB as Device
*************

In the device mode, the ESP32-S2 acts as an USB device, like a mouse or keyboard to be connected to a host device, like your computer or smartphone.

USB as Host
***********

The USB host mode, you can connect devices on the ESP32-S2, like external modems, mouse and keyboards.

.. note:: This mode is still under development for the ESP32-S2.

API Description
---------------

USB Common
**********

These are the common APIs for the USB driver.

onEvent
^^^^^^^

Event handling function to set the callback.

.. code-block:: arduino

    void onEvent(esp_event_handler_t callback);

Event handling function for the specific event.

.. code-block:: arduino

    void onEvent(arduino_usb_event_t event, esp_event_handler_t callback);

Where ``event`` can be:

* ARDUINO_USB_ANY_EVENT
* ARDUINO_USB_STARTED_EVENT
* ARDUINO_USB_STOPPED_EVENT
* ARDUINO_USB_SUSPEND_EVENT
* ARDUINO_USB_RESUME_EVENT
* ARDUINO_USB_MAX_EVENT

VID
^^^

Set the Vendor ID. This 16 bits identification is used to identify the company that develops the product.

.. note:: You can't define your own VID. If you need your own VID, you need to buy one. See https://www.usb.org/getting-vendor-id for more details.

.. code-block:: arduino

    bool VID(uint16_t v);


Get the Vendor ID.

.. code-block:: arduino

    uint16_t VID(void);

Returns the Vendor ID. The default value for the VID is: ``0x303A``.

PID
^^^

Set the Product ID. This 16 bits identification is used to identify the product.

.. code-block:: arduino

    bool PID(uint16_t p);

Get the Product ID.

.. code-block:: arduino

    uint16_t PID(void);

Returns the Product ID. The default PID is: ``0x0002``.

firmwareVersion
^^^^^^^^^^^^^^^

Set the firmware version. This is a 16 bits unsigned value.

.. code-block:: arduino
    
    bool firmwareVersion(uint16_t version);

Get the firmware version.

.. code-block:: arduino

    uint16_t firmwareVersion(void);

Return the 16 bits unsigned value. The default value is: ``0x100``.

usbVersion
^^^^^^^^^^

Set the USB version.

.. code-block:: arduino

    bool usbVersion(uint16_t version);

Get the USB version.

.. code-block:: arduino

    uint16_t usbVersion(void);

Return the USB version. The default value is: ``0x200`` (USB 2.0).

usbPower
^^^^^^^^

Set the USB power as mA (current).

.. note:: This configuration does not change the physical power output. This is only used for the USB device information.

.. code-block:: arduino

    bool usbPower(uint16_t mA);

Get the USB power configuration.

.. code-block:: arduino

    uint16_t usbPower(void);

Return the current in mA. The default value is: ``0x500`` (500mA).

usbClass
^^^^^^^^

Set the USB class.

.. code-block:: arduino

    bool usbClass(uint8_t _class);

Get the USB class.

.. code-block:: arduino

    uint8_t usbClass(void);

Return the USB class. The default value is: ``TUSB_CLASS_MISC``.

usbSubClass
^^^^^^^^^^^

Set the USB sub-class.

.. code-block:: arduino

    bool usbSubClass(uint8_t subClass);

Get the USB sub-class.

.. code-block:: arduino

    uint8_t usbSubClass(void);

Return the USB sub-class. The default value is: ``MISC_SUBCLASS_COMMON``.

usbProtocol
^^^^^^^^^^^

Define the USB protocol.

.. code-block:: arduino

    bool usbProtocol(uint8_t protocol);

Get the USB protocol.

.. code-block:: arduino

    uint8_t usbProtocol(void);

Return the USB protocol. The default value is: ``MISC_PROTOCOL_IAD``

usbAttributes
^^^^^^^^^^^^^

Set the USB attributes.

.. code-block:: arduino

    bool usbAttributes(uint8_t attr);

Get the USB attributes.

.. code-block:: arduino
    
    uint8_t usbAttributes(void);

Return the USB attributes. The default value is: ``TUSB_DESC_CONFIG_ATT_SELF_POWERED``

webUSB
^^^^^^

This function is used to enable the ``webUSB`` functionality.

.. code-block:: arduino

    bool webUSB(bool enabled);

This function is used to get the ``webUSB`` setting.

.. code-block:: arduino
    
    bool webUSB(void);

Return the ``webUSB`` setting (`Enabled` or `Disabled`)

productName
^^^^^^^^^^^

This function is used to define the product name.

.. code-block:: arduino

    bool productName(const char * name);

This function is used to get the product's name.

.. code-block:: arduino

    const char * productName(void);

manufacturerName
^^^^^^^^^^^^^^^^

This function is used to define the manufacturer name.

.. code-block:: arduino

    bool manufacturerName(const char * name);

This function is used to get the manufacturer's name.

.. code-block:: arduino
    
    const char * manufacturerName(void);

serialNumber
^^^^^^^^^^^^

This function is used to define the serial number.

.. code-block:: arduino

    bool serialNumber(const char * name);

This function is used to get the serial number.

.. code-block:: arduino

    const char * serialNumber(void);

The default serial number is: ``0``.

webUSBURL
^^^^^^^^^

This function is used to define the ``webUSBURL``.

.. code-block:: arduino

    bool webUSBURL(const char * name);

This function is used to get the ``webUSBURL``.

.. code-block:: arduino

    const char * webUSBURL(void);

The default ``webUSBURL`` is: https://espressif.github.io/arduino-esp32/webusb.html

enableDFU
^^^^^^^^^

This function is used to enable the DFU capability.

.. code-block:: arduino

    bool enableDFU();

begin
^^^^^

This function is used to start the peripheral using the default configuration.

.. code-block:: arduino

    bool begin();

USB MSC
*******

USB Mass Storage Class API. This class makes the device accessible as a mass storage device and allows you to transfer data between the host and the device.

One of the examples for this mode is to flash the device by dropping the firmware binary like a flash memory device when connecting the ESP32-S2 to the host computer.

begin
^^^^^

This function is used to start the peripheral using the default MSC configuration.

.. code-block:: arduino

    bool begin(uint32_t block_count, uint16_t block_size);

Where:

* ``block_count`` set the disk sector count.
* ``block_size`` set the disk sector size.

This function will return ``true`` if the configuration was successful.

end
^^^

This function will finish the peripheral as MSC and release all the allocated resources. After calling ``end`` you need to use ``begin`` again in order to initialize the USB MSC driver again.

.. code-block:: arduino

    void end();

vendorID
^^^^^^^^

This function is used to define the vendor ID.

.. code-block:: arduino

    void vendorID(const char * vid);//max 8 chars

productID
^^^^^^^^^

This function is used to define the product ID.

.. code-block:: arduino

    void productID(const char * pid);//max 16 chars

productRevision
^^^^^^^^^^^^^^^

This function is used to define the product revision.

.. code-block:: arduino

    void productRevision(const char * ver);//max 4 chars

mediaPresent
^^^^^^^^^^^^

Set the ``mediaPresent`` configuration.

.. code-block:: arduino

    void mediaPresent(bool media_present);

onStartStop
^^^^^^^^^^^

Set the ``onStartStop`` callback function.

.. code-block:: arduino

    void onStartStop(msc_start_stop_cb cb);

onRead
^^^^^^

Set the ``onRead`` callback function.

.. code-block:: arduino

    void onRead(msc_read_cb cb);

onWrite
^^^^^^^

Set the ``onWrite`` callback function.

.. code-block:: arduino

    void onWrite(msc_write_cb cb);

USB CDC
*******

USB Communications Device Class API. This class is used to enable communication between the host and the device.

This class is often used to enable serial communication and can be used to flash the firmware on the ESP32-S2 without the external USB to Serial chip.

onEvent
^^^^^^^

Event handling functions.

.. code-block:: arduino

    void onEvent(esp_event_handler_t callback);

.. code-block:: arduino

    void onEvent(arduino_usb_cdc_event_t event, esp_event_handler_t callback);

Where ``event`` can be:

* ARDUINO_USB_CDC_ANY_EVENT
* ARDUINO_USB_CDC_CONNECTED_EVENT
* ARDUINO_USB_CDC_DISCONNECTED_EVENT
* ARDUINO_USB_CDC_LINE_STATE_EVENT
* ARDUINO_USB_CDC_LINE_CODING_EVENT
* ARDUINO_USB_CDC_RX_EVENT
* ARDUINO_USB_CDC_TX_EVENT
* ARDUINO_USB_CDC_MAX_EVENT

setRxBufferSize
^^^^^^^^^^^^^^^

The ``setRxBufferSize`` function is used to set the size of the RX buffer.

.. code-block:: arduino

    size_t setRxBufferSize(size_t size);

setTxTimeoutMs
^^^^^^^^^^^^^^

This function is used to define the time to reach the timeout for the TX.

.. code-block:: arduino

    void setTxTimeoutMs(uint32_t timeout);

begin
^^^^^

This function is used to start the peripheral using the default CDC configuration.

.. code-block:: arduino

    void begin(unsigned long baud);

Where:

* ``baud`` is the baud rate.

end
^^^

This function will finish the peripheral as CDC and release all the allocated resources. After calling ``end`` you need to use ``begin`` again in order to initialize the USB CDC driver again.

.. code-block:: arduino

    void end();

available
^^^^^^^^^

This function will return if there are messages in the queue.

.. code-block:: arduino
    
    int available(void);

The return is the number of bytes available to read.

availableForWrite
^^^^^^^^^^^^^^^^^

This function will return if the hardware is available to write data.

.. code-block:: arduino

    int availableForWrite(void);

peek
^^^^

This function is used to ``peek`` messages from the queue.

.. code-block:: arduino

    int peek(void);

read
^^^^

This function is used to read the bytes available.

.. code-block:: arduino

    size_t read(uint8_t *buffer, size_t size);

Where:

* ``buffer`` is the pointer to the buffer to be read.
* ``size`` is the number of bytes to be read.

write
^^^^^

This function is used to write the message.

.. code-block:: arduino

    size_t write(const uint8_t *buffer, size_t size);

Where:

* ``buffer`` is the pointer to the buffer to be written.
* ``size`` is the number of bytes to be written.

flush
^^^^^

This function is used to flush the data.

.. code-block:: arduino

    void flush(void);

baudRate
^^^^^^^^

This function is used to get the ``baudRate``.

.. code-block:: arduino

    uint32_t baudRate();

setDebugOutput
^^^^^^^^^^^^^^

This function will enable the debug output, usually from the `UART0`, to the USB CDC.

.. code-block:: arduino

    void setDebugOutput(bool);

enableReboot
^^^^^^^^^^^^

This function enables the device to reboot by the DTR as RTS signals.

.. code-block:: arduino

    void enableReboot(bool enable);

rebootEnabled
^^^^^^^^^^^^^

This function will return if the reboot is enabled.

.. code-block:: arduino

    bool rebootEnabled(void);

Example Code
------------

There are a collection of USB device examples on the project GitHub, including Firmware MSC update, USB CDC, HID and composite device.

HIDVendor
*********

.. literalinclude:: ../../../libraries/USB/examples/HIDVendor/HIDVendor.ino
    :language: arduino

FirmwareMSC
***********

.. literalinclude:: ../../../libraries/USB/examples/FirmwareMSC/FirmwareMSC.ino
    :language: arduino

Mouse
*****

.. literalinclude:: ../../../libraries/USB/examples/Mouse/ButtonMouseControl/ButtonMouseControl.ino
    :language: arduino

.. _USB.org: https://www.usb.org/developers