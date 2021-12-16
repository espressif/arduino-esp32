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

ESP USB
^^^^^^^

.. code-block:: arduino

    ESPUSB(size_t event_task_stack_size=2048, uint8_t event_task_priority=5);

.. code-block:: arduino

    ~ESPUSB();

onEvent
^^^^^^^

.. code-block:: arduino

    void onEvent(esp_event_handler_t callback);

.. code-block:: arduino

    void onEvent(arduino_usb_event_t event, esp_event_handler_t callback);

VID
^^^

.. code-block:: arduino

    bool VID(uint16_t v);

.. code-block:: arduino

    uint16_t VID(void);

PID
^^^

.. code-block:: arduino

    bool PID(uint16_t p);

.. code-block:: arduino

    uint16_t PID(void);

firmwareVersion
^^^^^^^^^^^^^^^

.. code-block:: arduino
    
    bool firmwareVersion(uint16_t version);

.. code-block:: arduino

    uint16_t firmwareVersion(void);

usbVersion
^^^^^^^^^^

.. code-block:: arduino

    bool usbVersion(uint16_t version);

.. code-block:: arduino

    uint16_t usbVersion(void);

usbPower
^^^^^^^^

.. code-block:: arduino

    bool usbPower(uint16_t mA);

.. code-block:: arduino

    uint16_t usbPower(void);

usbClass
^^^^^^^^

.. code-block:: arduino

    bool usbClass(uint8_t _class);

.. code-block:: arduino

    uint8_t usbClass(void);

usbSubClass
^^^^^^^^^^^

.. code-block:: arduino

    bool usbSubClass(uint8_t subClass);

.. code-block:: arduino

    uint8_t usbSubClass(void);

usbProtocol
^^^^^^^^^^^

.. code-block:: arduino

    bool usbProtocol(uint8_t protocol);

.. code-block:: arduino

    uint8_t usbProtocol(void);

usbAttributes
^^^^^^^^^^^^^

.. code-block:: arduino

    bool usbAttributes(uint8_t attr);

.. code-block:: arduino
    
    uint8_t usbAttributes(void);

webUSB
^^^^^^

.. code-block:: arduino

    bool webUSB(bool enabled);

.. code-block:: arduino
    
    bool webUSB(void);

productName
^^^^^^^^^^^

.. code-block:: arduino

    bool productName(const char * name);
    
.. code-block:: arduino

    const char * productName(void);

manufacturerName
^^^^^^^^^^^^^^^^

.. code-block:: arduino

    bool manufacturerName(const char * name);
    
.. code-block:: arduino
    
    const char * manufacturerName(void);

serialNumber
^^^^^^^^^^^^

.. code-block:: arduino

    bool serialNumber(const char * name);
    
.. code-block:: arduino

    const char * serialNumber(void);

webUSBURL
^^^^^^^^^

.. code-block:: arduino

    bool webUSBURL(const char * name);

.. code-block:: arduino

    const char * webUSBURL(void);

enableDFU
^^^^^^^^^

.. code-block:: arduino

    bool enableDFU();

begin
^^^^^

.. code-block:: arduino

    bool begin();

USB MSC
*******

USBMSC
^^^^^^

.. code-block:: arduino

    USBMSC();

.. code-block:: arduino

    ~USBMSC();

begin
^^^^^

.. code-block:: arduino

    bool begin(uint32_t block_count, uint16_t block_size);

end
^^^

.. code-block:: arduino

    void end();

vendorID
^^^^^^^^

.. code-block:: arduino

    void vendorID(const char * vid);//max 8 chars

productID
^^^^^^^^^

.. code-block:: arduino

    void productID(const char * pid);//max 16 chars

productRevision
^^^^^^^^^^^^^^^

.. code-block:: arduino

    void productRevision(const char * ver);//max 4 chars

mediaPresent
^^^^^^^^^^^^

.. code-block:: arduino

    void mediaPresent(bool media_present);

onStartStop
^^^^^^^^^^^

.. code-block:: arduino

    void onStartStop(msc_start_stop_cb cb);

onRead
^^^^^^

.. code-block:: arduino

    void onRead(msc_read_cb cb);

onWrite
^^^^^^^

.. code-block:: arduino

    void onWrite(msc_write_cb cb);

USB CDC
*******

USBCDC
^^^^^^

USBCDC
^^^^^^

.. code-block:: arduino

    USBCDC(uint8_t itf=0);

.. code-block:: arduino

    ~USBCDC();

onEvent
^^^^^^^

.. code-block:: arduino

    void onEvent(esp_event_handler_t callback);

.. code-block:: arduino

    void onEvent(arduino_usb_cdc_event_t event, esp_event_handler_t callback);

setRxBufferSize
^^^^^^^^^^^^^^^

.. code-block:: arduino

    size_t setRxBufferSize(size_t size);

setTxTimeoutMs
^^^^^^^^^^^^^^

.. code-block:: arduino

    void setTxTimeoutMs(uint32_t timeout);

begin
^^^^^

.. code-block:: arduino

    void begin(unsigned long baud=0);

end
^^^

.. code-block:: arduino

    void end();

available
^^^^^^^^^

.. code-block:: arduino
    
    int available(void);

availableForWrite
^^^^^^^^^^^^^^^^^

.. code-block:: arduino

    int availableForWrite(void);

peek
^^^^

.. code-block:: arduino

    int peek(void);

read
^^^^

.. code-block:: arduino

    int read(void);

.. code-block:: arduino

    size_t read(uint8_t *buffer, size_t size);

write
^^^^^

.. code-block:: arduino

    size_t write(uint8_t);

.. code-block:: arduino

    size_t write(const uint8_t *buffer, size_t size);

flush
^^^^^

.. code-block:: arduino

    void flush(void);

baudRate
^^^^^^^^

.. code-block:: arduino

    uint32_t baudRate();

setDebugOutput
^^^^^^^^^^^^^^

.. code-block:: arduino

    void setDebugOutput(bool);

enableReboot
^^^^^^^^^^^^

.. code-block:: arduino

    void enableReboot(bool enable);

rebootEnabled
^^^^^^^^^^^^^

.. code-block:: arduino

    bool rebootEnabled(void);

Example Code
------------

There ara a collection of USB device examples on the project GitHub, including Firmware MSC update, USB CDC, HID and composite device.

HIDVendor
*********

.. literalinclude:: ../../../libraries/USB/examples/HIDVendor/HIDVendor.ino
    :language: arduino

FirmwareMSC
***********

.. literalinclude:: ../../../libraries/USB/examples/FirmwareMSC/FirmwareMSC.ino
    :language: arduino



.. _USB.org: https://www.usb.org/developers