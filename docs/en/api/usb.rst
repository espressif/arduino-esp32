#######
USB API
#######

.. note:: This feature is only supported on ESP chips that have USB peripheral, like the ESP32-S2 and ESP32-S3. Some chips, like the ESP32-C3 include native CDC+JTAG peripheral that is not covered here.

About
-----

The **Universal Serial Bus** is a widely used peripheral to exchange data between devices. USB was introduced on the ESP32, supporting both device and host mode.

To learn about the USB, see the `USB.org`_ for developers.

USB as Device
*************

In the device mode, the ESP32 acts as an USB device, like a mouse or keyboard to be connected to a host device, like your computer or smartphone.

USB as Host
***********

The USB host mode, you can connect devices on the ESP32, like external modems, mouse and keyboards.

.. note:: This mode is still under development for the ESP32.

API Description
---------------

This is the common USB API description.

For more supported USB classes implementation, see the following sections:

.. toctree::
   :maxdepth: 1
   :caption: Classes:

   USB CDC <usb_cdc>
   USB MSC <usb_msc>

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

Example Code
------------

There are a collection of USB device examples on the project GitHub, including Firmware MSC update, USB CDC, HID and composite device.

.. _USB.org: https://www.usb.org/developers
