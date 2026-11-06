############
USB Host API
############

.. note:: Supported on ESP32-S2, ESP32-S3 and ESP32-P4, the targets that have the USB OTG peripheral. Chips with only a native CDC+JTAG peripheral, such as the ESP32-C3, cannot act as a USB host.

About
-----

The ``USBHost`` API lets the ESP32 drive USB peripherals: HID devices (mice, keyboards, gamepads), CDC serial adapters and mass storage sticks. It is built on the **TinyUSB** host stack.

The ESP-IDF USB Host Library (``usb/usb_host.h``) is a second, independent host stack. Only one of the two can own the USB peripheral at a time, so a sketch picks one or the other. See :doc:`usb` for when to reach for the ESP-IDF stack instead.

Hardware and IDE setup
----------------------

The host stack needs the OTG peripheral, so the TinyUSB *device* stack must not be holding it.

On **ESP32-S3** the USB_SERIAL_JTAG and USB_OTG blocks share one PHY, so do not combine **Tools → USB Mode: USB-OTG (TinyUSB)** with **Tools → USB CDC On Boot: Enabled**. Either setting works on its own:

* **Hardware CDC and JTAG** puts ``Serial`` on the native USB port.
* **USB-OTG** with CDC on boot disabled puts ``Serial`` on UART0.

On boards where the two menus are tied together, such as **ESP32-S3-USB-OTG**, either choice is fine as-is. **ESP32-P4** drives host and device from separate controllers, so any combination works there.

A host port also has to supply VBUS. ``USBHost.begin()`` calls ``USBHostBoardInit()``, a weak hook whose default is empty; a variant overrides it to switch the bus mux and enable the 5 V supply. See ``variants/esp32s3usbotg`` for a working example.

.. code-block:: arduino

    extern "C" void USBHostBoardInit(void);

If a device misbehaves when attached directly, putting a USB hub between the board and the device often helps.

Sketch structure
----------------

Who owns which instance
***********************

The library provides only the objects that the stack itself needs. Everything else is declared by the sketch, so a program links in just the handlers it actually names.

.. list-table::
   :header-rows: 1
   :widths: 30 20 50

   * - Object
     - Provided by
     - Notes
   * - ``USBHost``
     - library
     - The controller. Always available.
   * - ``USBHostHID``
     - library
     - Dispatcher that routes HID interfaces to handlers.
   * - ``USBHostMSC``
     - library
     - Block device. The FatFs layer needs a fixed instance.
   * - ``USBHostHIDMouse``
     - sketch
     - e.g. ``USBHostHIDMouse USBHostMouse;``
   * - ``USBHostHIDKeyboard``
     - sketch
     - e.g. ``USBHostHIDKeyboard USBHostKeyboard;``
   * - ``USBHostHIDGamepad``
     - sketch
     - e.g. ``USBHostHIDGamepad USBHostGamepad;``
   * - ``USBHostSerialClass``
     - sketch
     - e.g. ``USBHostSerialClass USBHostSerial;``
   * - ``USBMSCFSClass``
     - sketch
     - e.g. ``USBMSCFSClass USBMSCFS;``

Minimal sketch
**************

Every HID handler must call ``registerWithHost()`` **before** ``USBHost.begin()``, otherwise it will not be offered any interface.

.. code-block:: arduino

    #include <USBHostHIDMouse.h>

    USBHostHIDMouse USBHostMouse;

    void setup() {
      Serial.begin(115200);
      USBHostMouse.registerWithHost();
      USBHost.begin();
    }

    void loop() {
      USBHost.task();
      if (USBHostMouse.available()) {
        Serial.printf("x=%d y=%d buttons=0x%02X wheel=%d\n",
                      USBHostMouse.getX(), USBHostMouse.getY(),
                      USBHostMouse.getButtons(), USBHostMouse.getWheel());
      }
    }

Threading model
***************

After ``begin()``, TinyUSB runs on a background worker task. ``USBHost.task()`` in ``loop()`` dispatches the deferred report callbacks and, when no worker is running, also services TinyUSB itself.

Report callbacks therefore run in ``loop()`` context, not in the TinyUSB worker. ``Serial`` is safe inside them, but never call ``tuh_*`` functions from a callback.

USBHost
-------

begin
*****

Starts host mode: brings up the board hook, the TinyUSB host stack and the background worker. Returns ``true`` on success.

.. code-block:: arduino

    bool begin();

task
****

Dispatches pending HID report callbacks. Call it from ``loop()`` on every iteration.

.. code-block:: arduino

    void task();

setCore / core
**************

Pins the TinyUSB host worker to a core. Call before ``begin()``. The default is ``ARDUINO_USB_HOST_CORE`` (0); pass ``-1`` to leave the task unpinned.

.. code-block:: arduino

    void setCore(int coreId);
    int core() const;

started
*******

True once ``begin()`` has succeeded. The class also converts to ``bool``.

.. code-block:: arduino

    bool started() const;

tuhBackgroundActive
*******************

True when TinyUSB is being serviced by the background worker. While it is true, a sketch must not call ``tuh_*`` from ``loop()``.

.. code-block:: arduino

    bool tuhBackgroundActive() const;

HID
---

``USBHostHID`` is the dispatcher. Each attached HID interface is offered to the registered handlers in registration order, and the first one that claims it owns it.

.. warning:: Register the gamepad handler **before** the mouse handler when both are used. Many gamepads present a report descriptor that also satisfies the report-protocol mouse heuristics, so a mouse registered first would claim the pad.

.. code-block:: arduino

    bool addDevice(USBHostHIDDevice *dev);
    void serviceReceives();
    bool mounted() const;

Mouse
*****

Claims boot-protocol mice and most report-protocol mice. Boot protocol is switched to report protocol on claim, because boot mouse reports carry no wheel byte. Common 8-bit and 16-bit X/Y layouts are handled.

.. code-block:: arduino

    bool available();
    int16_t getX() const;
    int16_t getY() const;
    uint8_t getButtons() const;
    int8_t getWheel() const;
    void clear();
    void registerWithHost();
    void setReportCallback(USBHostHIDMouseReportCb cb, void *arg = nullptr);

``available()`` consumes the pending report, so read the getters after it returns ``true``. The callback form receives the same data:

.. code-block:: arduino

    void onMouse(int16_t x, int16_t y, uint8_t buttons, int8_t wheel, void *arg);

Keyboard
********

Claims boot-protocol keyboard interfaces and parses the 8-byte boot report.

.. code-block:: arduino

    bool available();
    uint8_t getModifiers() const;
    void getKeys(uint8_t keys[6]) const;
    bool isKeyDown(uint8_t hid_usage) const;
    void clear();
    void setNotifyOnChangeOnly(bool enable);
    bool notifyOnChangeOnly() const;
    void registerWithHost();
    void setReportCallback(USBHostHIDKeyboardReportCb cb, void *arg = nullptr);

``setNotifyOnChangeOnly(true)`` suppresses repeated reports for keys that are simply held down.

Decoding
^^^^^^^^

``toAscii()`` applies the modifier and lock state to produce text, using a ``USBHIDKeyboard`` layout (US by default, see the ``KeyboardLayout_*`` tables).

.. code-block:: arduino

    size_t toAscii(char *buf, size_t cap, const uint8_t *layout = KeyboardLayout_en_US) const;
    size_t toAscii(char *buf, size_t cap, uint8_t modifiers, const uint8_t keys[6],
                   const uint8_t *layout = KeyboardLayout_en_US) const;
    uint8_t toVirtualKey(uint8_t hid_usage) const;
    const char *toVirtualKeyName(uint8_t hid_usage) const;
    void printReport(Print &out, uint8_t modifiers, const uint8_t keys[6],
                     const uint8_t *layout = KeyboardLayout_en_US) const;

``toVirtualKey()`` maps a keyboard-page usage to an Arduino ``KEY_*`` value, returning 0 when the usage is not a special key. ``printReport()`` writes a human-readable line such as ``LEFT_CTRL+KEY_F1``.

Lock keys and LEDs
^^^^^^^^^^^^^^^^^^

The host owns the lock state and mirrors it onto the keyboard LEDs by sending an output report. This is on by default, so pressing Caps Lock lights the LED and changes what ``toAscii()`` returns; Num Lock likewise gates the keypad digits. A sketch normally only reads the state.

.. code-block:: arduino

    uint8_t getLeds() const;
    void setLeds(uint8_t leds);
    bool capsLock() const;
    bool numLock() const;
    bool scrollLock() const;
    void setLockHandling(bool enable);
    bool lockHandling() const;

``setLeds()`` forces the state, for example ``setLeds(USBHOST_KEY_LED_NUM_LOCK)`` to start with the keypad live. ``setLockHandling(false)`` hands the whole thing back to the sketch: the keys then report as plain presses and the LEDs stay dark.

The bitmasks are ``USBHOST_KEY_LED_NUM_LOCK``, ``USBHOST_KEY_LED_CAPS_LOCK``, ``USBHOST_KEY_LED_SCROLL_LOCK``, ``USBHOST_KEY_LED_COMPOSE`` and ``USBHOST_KEY_LED_KANA``.

Gamepad
*******

Gamepad report layouts are device-specific, so this handler exposes the raw report and a couple of heuristics rather than pretending to normalize every pad.

.. code-block:: arduino

    bool available();
    void clear();
    uint16_t reportLength() const;
    const uint8_t *reportData() const;
    uint16_t getReport(uint8_t *dst, uint16_t max_len) const;
    uint16_t getButtons16() const;
    void getSticks8(int8_t *lx, int8_t *ly, int8_t *rx, int8_t *ry, bool skip_id_byte = false) const;
    void setNotifyOnChangeOnly(bool on);
    void registerWithHost();
    void setReportCallback(USBHostHIDGamepadReportCb cb, void *arg = nullptr);

Reports are capped at ``USBHostHIDGamepad::REPORT_CAP`` (64) bytes. ``getButtons16()`` reads the first two bytes as a little-endian button mask, and ``getSticks8()`` converts unsigned centre-128 axes to signed values.

Inspecting an unknown device
****************************

``USBHostHIDReportMapDumper`` prints the parsed report descriptor of every interface and never claims anything, which makes it useful when a device is not picked up by the handler you expected. Register it before the real handlers.

.. code-block:: arduino

    USBHostHIDReportMapDumper dumper;   // defaults to Serial

    void setup() {
      dumper.registerWithHost();
      // ... the real handlers ...
      USBHost.begin();
    }

The parser is also callable directly from ``USBHIDReportMapParse.h`` through ``usbhid_parse_report_map()``, ``usbhid_print_parsed_report_map()`` and ``usbhid_free_report_map()``.

Writing a custom handler
************************

Subclass ``USBHostHIDDevice`` and implement the three pure virtuals. Return ``true`` from ``claim()`` to take the interface.

.. code-block:: arduino

    class MyPad : public USBHostHIDDevice {
      bool claim(uint8_t dev_addr, uint8_t idx, uint8_t protocol,
                 const uint8_t *report_desc, uint16_t desc_len) override;
      void onUnmount(uint8_t dev_addr, uint8_t idx) override;
      void onReport(uint8_t dev_addr, uint8_t idx, const uint8_t *report, uint16_t len) override;
    };

.. note:: ``onReport()`` runs in the TinyUSB worker context. Keep it short and do not call ``tuh_*`` from it; hand data to ``loop()`` instead, which is what the bundled handlers do.

CDC host
--------

``USBHostSerialClass`` is a ``Stream``, so it behaves like ``Serial`` once a CDC adapter is attached.

.. code-block:: arduino

    USBHostSerialClass USBHostSerial;

    void begin(unsigned long baud = 0);
    void end();
    bool mounted() const;
    uint32_t baudRate();
    void setTxTimeoutMs(uint32_t timeout);
    uint32_t getTxTimeoutMs() const;

``begin(0)`` skips the automatic ``SET_LINE_CODING``, leaving the adapter at whatever it was configured to. The object converts to ``bool``, so ``if (USBHostSerial)`` tests the mount state.

On top of the usual ``Stream`` methods, writes honor a configurable timeout: ``setTxTimeoutMs(0)`` makes ``write()`` non-blocking, returning what fit.

.. note:: ``CFG_TUH_CDC`` sets the host CDC pool size and is often 1 in the prebuilt libraries, which means one CDC device at a time.

MSC host
--------

Two layers: ``USBHostMSC`` is the block device, and ``USBMSCFSClass`` puts a FatFs filesystem on top of it with the same API as ``SD``.

Block device
************

.. code-block:: arduino

    bool mounted() const;
    uint8_t devAddr() const;
    uint8_t lun() const;
    uint32_t blockCount() const;
    uint32_t blockSize() const;
    bool readBlocks(uint32_t lba, void *buffer, uint32_t blocks);
    bool writeBlocks(uint32_t lba, const void *buffer, uint32_t blocks);

``readBlocks()`` and ``writeBlocks()`` are synchronous SCSI READ(10) / WRITE(10) and block until the transfer finishes.

Filesystem
**********

.. code-block:: arduino

    USBMSCFSClass USBMSCFS;

    bool begin(const char *mountpoint = "/usb", uint8_t max_files = 5, bool format_if_empty = false);
    void end();
    uint64_t cardSize();
    size_t numSectors();
    size_t sectorSize();
    uint64_t totalBytes();
    uint64_t usedBytes();
    bool readRAW(uint8_t *buffer, uint32_t sector);
    bool writeRAW(uint8_t *buffer, uint32_t sector);

Wait for ``USBHostMSC.mounted()`` before calling ``begin()``. FAT16 and FAT32 are supported, on an MBR partition table or on GPT with a Microsoft Basic Data partition.

.. warning:: Paths are volume-relative, not prefixed with the mount point: open ``/file.txt``, not ``/usb/file.txt``. Call ``end()`` before unplugging the drive, otherwise unwritten data is lost.

.. code-block:: arduino

    if (USBHostMSC.mounted() && USBMSCFS.begin("/usb")) {
      File f = USBMSCFS.open("/hello.txt", FILE_WRITE);
      f.println("Hello");
      f.close();
      USBMSCFS.end();
    }

Troubleshooting
---------------

Set **Tools → Core Debug Level → Verbose** to get traces from the host stack: ``[USBHostHID]`` for claim and mount decisions, ``[USBHostMSC]`` for SCSI submit, wait, timeout and CSW errors, and ``[USBMSCFS]`` for disk I/O.

If a HID device enumerates but never reports, dump its descriptor with ``USBHostHIDReportMapDumper`` and check that the handler you expected actually claims it.

Examples
--------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Example
     - Shows
   * - ``USBHostMouse``
     - Polling a mouse.
   * - ``USBHostMouseCallback``
     - The same, driven by a report callback.
   * - ``USBHostKeyboard``
     - Keys, ASCII decoding, lock keys and LEDs.
   * - ``USBHostGamepad``
     - Raw gamepad reports.
   * - ``USBHostHIDCombo``
     - Mouse, keyboard and gamepad together, plus descriptor dumps.
   * - ``USBHostSerial``
     - CDC adapter through the ``Stream`` API.
   * - ``USBHostMSC_Test``
     - The ``SD_Test`` exercises on a USB flash drive.
   * - ``USBHostCDC_MSC_HID``
     - All three classes in one sketch.
   * - ``USBHostIDF``
     - The ESP-IDF host stack instead of TinyUSB.
