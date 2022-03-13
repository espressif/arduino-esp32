#######
USB CDC
#######

About
-----

USB Communications Device Class API. This class is used to enable communication between the host and the device.

This class is often used to enable serial communication and can be used to flash the firmware on the ESP32 without the external USB to Serial chip.

APIs
****

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

Here is an example of how to use the USB CDC.

USBSerial
*********

.. literalinclude:: ../../../libraries/USB/examples/USBSerial/USBSerial.ino
    :language: arduino

.. _USB.org: https://www.usb.org/developers
