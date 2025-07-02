#######
USB MSC
#######

About
-----

USB Mass Storage Class API. This class makes the device accessible as a mass storage device and allows you to transfer data between the host and the device.

One of the examples for this mode is to flash the device by dropping the firmware binary like a flash memory device when connecting the ESP32 to the host computer.

APIs
****

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

Example Code
------------

Here is an example of how to use the USB MSC.

FirmwareMSC
***********

.. literalinclude:: ../../../libraries/USB/examples/FirmwareMSC/FirmwareMSC.ino
    :language: arduino

.. _USB.org: https://www.usb.org/developers
