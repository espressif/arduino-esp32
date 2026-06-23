###########################
ESP32-P4X-Function-EV-Board
###########################

The `ESP32-P4X-Function-EV-Board`_ (v1.6) is one of Espressif's official development boards, based on the `ESP32-P4`_ SoC (chip revision v3.1 or later).

.. note::
    The older ESP32-P4-Function-EV-Board revisions (v1.4 and v1.5.2) are End-of-Life (EOL).
    See `Previous Revisions (EOL)`_ for hardware differences.

.. warning::
    The ESP32-P4 chip revision v3.1 does **not** support Secure Download. Do not enable Secure Download Mode. See `ESP32-P4 Series SoC Errata`_ > ROM-770 for details.

Specifications
--------------

- 400 MHz Dual Core RISC-V CPU with AI extensions
- 40 MHz Ultra-Low-Power Co-processor
- Single-precision FPU
- Integrated 16 MB SPI flash
- PSRAM support
- Peripherals
    - UART
    - SPI
    - I2C
    - I2S
    - SDIO
    - LED PWM
    - MIPI CSI/DSI
    - USB Serial/JTAG
    - USB OTG Full-Speed
    - USB OTG High-Speed
    - Ethernet (EMAC RMII)
    - TWAI (CAN)
    - GPIO

USB Architecture
----------------

The ESP32-P4 has **three independent USB controllers**, each with its own dedicated PHY. This is a key difference from the ESP32-S3, which shares a single PHY between USB Serial/JTAG and USB OTG.

.. list-table::
    :header-rows: 1
    :widths: 25 20 20 20 15

    * - Controller
      - Speed
      - PHY
      - Pins
      - Board Port
    * - USB Serial/JTAG
      - Full-Speed (12 Mbps)
      - FS_PHY1
      - GPIO24 (D-) / GPIO25 (D+)
      - USB Serial/JTAG Port
    * - USB OTG Full-Speed
      - Full-Speed (12 Mbps)
      - FS_PHY2
      - GPIO26 (D-) / GPIO27 (D+)
      - USB Full-speed Port
    * - USB OTG High-Speed
      - High-Speed (480 Mbps)
      - HS_PHY
      - pin49 (D-) / pin50 (D+)
      - USB 2.0 Type-C / Type-A Port

Because each controller has a **dedicated PHY**, all three can operate **simultaneously** without conflicts.

USB Ports on the Board
^^^^^^^^^^^^^^^^^^^^^^

The board has four USB connectors:

.. list-table::
    :header-rows: 1
    :widths: 30 15 55

    * - Port
      - Connector
      - Description
    * - USB Serial/JTAG Port
      - Type-C
      - Connected to the built-in USB Serial/JTAG controller (GPIO24/25). Used for flashing, serial console, and JTAG debugging.
    * - USB Full-speed Port
      - Type-C
      - Connected to the USB OTG Full-Speed controller (GPIO26/27). Available for USB device/host applications at full-speed.
    * - USB 2.0 Type-C Port
      - Type-C
      - Connected to the USB OTG High-Speed controller (pin49/50). Used for USB device applications at high-speed. Cannot be used simultaneously with the USB 2.0 Type-A Port.
    * - USB 2.0 Type-A Port
      - Type-A
      - Connected to the USB OTG High-Speed controller (pin49/50). Used for USB host applications. Cannot be used simultaneously with the USB 2.0 Type-C Port.

Arduino USB Configuration
-------------------------

The Arduino framework uses two build flags to control USB behavior:

- ``ARDUINO_USB_MODE``: Selects the USB stack (``1`` = Hardware CDC, ``0`` = USB-OTG / TinyUSB)
- ``ARDUINO_USB_CDC_ON_BOOT``: Controls whether ``Serial`` maps to a USB CDC interface (``1`` = Enabled, ``0`` = Disabled, maps to UART0)

These are configured in the Arduino IDE via the **USB Mode** and **USB CDC on Boot** menu options.

Serial Mapping
^^^^^^^^^^^^^^

.. list-table::
    :header-rows: 1
    :widths: 20 20 25 35

    * - USB Mode
      - CDC on Boot
      - ``Serial`` maps to
      - Output visible on
    * - Hardware CDC (default)
      - Enabled (default)
      - ``HWCDCSerial``
      - USB Serial/JTAG Port
    * - Hardware CDC
      - Disabled
      - ``Serial0`` (UART0)
      - UART0 GPIO pins (not on USB)
    * - USB-OTG (TinyUSB)
      - Enabled
      - ``USBSerial``
      - USB 2.0 Type-C Port (High-Speed)
    * - USB-OTG (TinyUSB)
      - Disabled
      - ``Serial0`` (UART0)
      - UART0 GPIO pins (not on USB)

.. important::
    The USB Serial/JTAG Controller is a **fixed-function hardware device** that always provides a CDC-ACM serial port, independently of any Arduino USB settings. ``printf()``, ESP-IDF log output, ``esptool`` flashing, and JTAG debugging always work through the USB Serial/JTAG Port regardless of the configuration above.

    The **USB Mode** and **USB CDC on Boot** settings only control what the Arduino ``Serial`` object maps to. They do not affect the USB Serial/JTAG hardware itself.

Serial Output Architecture
^^^^^^^^^^^^^^^^^^^^^^^^^^

The ESP32-P4 has **two independent output paths** that both reach the USB Serial/JTAG port. Understanding the difference is important:

**ESP-IDF Layer** (``printf()`` / ``ESP_LOGI()``):
    Controlled by ``CONFIG_ESP_CONSOLE_*`` in the ESP-IDF sdkconfig. The default configuration uses UART0 as the primary console and USB Serial/JTAG as a **secondary console** (output-only). This means ``printf()`` output always appears on the USB Serial/JTAG Port automatically, with no Arduino settings required.

**Arduino Layer** (``Serial.println()``):
    Controlled by ``ARDUINO_USB_CDC_ON_BOOT`` and ``ARDUINO_USB_MODE``. These compile-time flags determine which C++ object the ``Serial`` macro maps to (``HWCDCSerial``, ``USBSerial``, or ``Serial0``). This is completely independent of the IDF console configuration.

.. note::
    The IDF's secondary console uses a lightweight ROM-level polling mechanism that does not install an interrupt handler. Arduino's ``HWCDCSerial`` (the ``HWCDC`` class) uses its own ISR on the USB Serial/JTAG interrupt source. These two coexist safely.

    Setting ``CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`` as the **primary** console would install a full IDF VFS driver with its own ISR, which would conflict with ``HWCDC``'s ISR. This is why the Arduino sdkconfig correctly uses UART0 as primary and USB Serial/JTAG as secondary.

In practice:

- ``printf("hello")`` -- always visible on the USB Serial/JTAG Port (via IDF secondary console)
- ``Serial.println("hello")`` -- visible on whichever port ``Serial`` is mapped to (depends on **USB Mode** and **CDC on Boot**)

Default Configuration
^^^^^^^^^^^^^^^^^^^^^

The default configuration for the ESP32-P4 Dev Module is:

- **USB Mode**: Hardware CDC and JTAG (``ARDUINO_USB_MODE=1``)
- **USB CDC on Boot**: Enabled (``ARDUINO_USB_CDC_ON_BOOT=1``)
- **Upload Mode**: UART0 / Hardware CDC

This maps ``Serial`` to ``HWCDCSerial``, so ``Serial.println()`` output is visible on the **USB Serial/JTAG Port**. Upload and JTAG debugging also use this port.

.. note::
    This board does not have a USB-to-UART bridge chip, so UART0 is only accessible via the GPIO header pins (GPIO37 = U0TXD, GPIO38 = U0RXD). With **USB CDC on Boot** disabled, ``Serial.println()`` would go to UART0 (invisible over USB), while ``printf()`` output would still appear on the USB Serial/JTAG Port as usual.

When to Use Each USB Port and Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Choosing a USB Port:**

- **USB Serial/JTAG Port** (GPIO24/25): Use for flashing, JTAG debugging, and serial console. This port is always available and is the recommended default for development.
- **USB Full-Speed Port** (GPIO26/27): Use for USB device/host applications at full-speed (12 Mbps), such as HID, MSC, or CDC devices. This port uses the OTG Full-Speed controller and is independent of the USB Serial/JTAG Port.
- **USB 2.0 Type-C / Type-A Port** (pin49/50): Use for high-speed USB device/host applications (480 Mbps). The Type-C connector is for device mode, the Type-A connector is for host mode. They share the same controller and cannot be used simultaneously.

**Choosing a USB Mode:**

- **Hardware CDC and JTAG** (default): ``Serial`` maps to ``HWCDCSerial``, which communicates through the USB Serial/JTAG controller. Use this mode for serial communication over the USB Serial/JTAG Port. ``HWCDCSerial`` is an Arduino ``Stream`` object backed by the ``HWCDC`` class, which talks directly to the USB Serial/JTAG hardware registers using a custom ISR and ring buffers.
- **USB-OTG (TinyUSB)**: ``Serial`` maps to ``USBSerial``, which communicates through the USB OTG High-Speed controller via the TinyUSB stack. Use this mode when you want serial output on the USB 2.0 Type-C Port, or when building USB device applications (HID, MSC, MIDI, vendor-specific, etc.).

.. note::
    Both USB modes can be used independently of the USB Serial/JTAG Port's flashing and debugging capabilities. Regardless of the selected USB Mode, you can always flash and debug through the USB Serial/JTAG Port. The USB Mode setting only controls what the Arduino ``Serial`` object maps to.

Using USB-OTG (TinyUSB) Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To use the USB-OTG mode with TinyUSB for ``Serial`` and upload:

1. Set **USB Mode** to ``USB-OTG (TinyUSB)``
2. Set **USB CDC on Boot** to ``Enabled``
3. Set **Upload Mode** to ``USB-OTG CDC (TinyUSB)``
4. Connect to the **USB 2.0 Type-C Port** (High-Speed, pin49/50)

Arduino's TinyUSB stack on the ESP32-P4 uses the **High-Speed** OTG controller. The upload flow uses a 1200bps touch to reset the board into download mode.

.. note::
    The first flash must be done via the **USB Serial/JTAG Port** (or by manually entering download mode with BOOT + RESET), since TinyUSB CDC is not available on a blank chip. Subsequent uploads can use the USB 2.0 Type-C Port.

Comparison with ESP32-S3
^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
    :header-rows: 1
    :widths: 35 30 35

    * - Feature
      - ESP32-S3
      - ESP32-P4
    * - USB Full-Speed PHYs
      - 1 (shared between USJ and OTG)
      - 2 (dedicated per controller)
    * - USB High-Speed
      - No
      - Yes (480 Mbps)
    * - Simultaneous USJ + OTG
      - No (shared PHY)
      - Yes (separate PHYs)
    * - Default ``USB Mode``
      - Hardware CDC
      - Hardware CDC
    * - Default ``CDC on Boot``
      - Disabled (has UART bridge)
      - Enabled (no UART bridge)

On the ESP32-S3, ``USB Mode`` controls which peripheral gets the shared PHY. On the ESP32-P4, both peripherals are always active on their own PHYs — ``USB Mode`` only controls what ``Serial`` maps to.

Previous Revisions (EOL)
^^^^^^^^^^^^^^^^^^^^^^^^

The following revisions of the ESP32-P4-Function-EV-Board are End-of-Life:

.. list-table::
    :header-rows: 1
    :widths: 15 30 30 25

    * - Revision
      - Chip
      - Key Differences from P4X
      - User Guide
    * - v1.5.2
      - ESP32-P4 rev v3.0
      - Same board layout and USB ports; older chip revision
      - `ESP32-P4-Function-EV-Board User Guide (v1.5.2)`_
    * - v1.4
      - ESP32-P4 (pre-v3.0)
      - CP2102N USB-to-UART bridge instead of USB Serial/JTAG Port; no USB Full-speed Port connector; GPIO24/25 on header
      - `ESP32-P4-Function-EV-Board User Guide (v1.4)`_

**v1.4 hardware differences:**

On v1.4, the CP2102N bridge routes UART0 to USB, so ``Serial.println()`` output is visible even with ``CDC on Boot = Disabled`` (``Serial`` maps to ``Serial0`` = UART0, which goes through the bridge). All four combinations of USB Mode and CDC on Boot produce visible serial output on v1.4.

Header Block
------------

J1
^^

===  ======  =====  ===================================
No.  Name    Type   Function
===  ======  =====  ===================================
1    3V3     P      3.3 V power supply
2    5V      P      5 V power supply
3    7       I/O/T  GPIO7
4    5V      P      5 V power supply
5    8       I/O/T  GPIO8
6    GND     GND    Ground
7    23      I/O/T  GPIO23
8    37      I/O/T  U0TXD, GPIO37
9    GND     GND    Ground
10   38      I/O/T  U0RXD, GPIO38
11   21      I/O/T  GPIO21
12   22      I/O/T  GPIO22
13   20      I/O/T  GPIO20
14   GND     GND    Ground
15   6       I/O/T  GPIO6
16   5       I/O/T  GPIO5
17   3V3     P      3.3 V power supply
18   4       I/O/T  GPIO4
19   3       I/O/T  GPIO3
20   GND     GND    Ground
21   2       I/O/T  GPIO2
22   NC(1)   I/O/T  GPIO1 [1]_
23   NC(0)   I/O/T  GPIO0 [1]_
24   36      I/O/T  GPIO36
25   GND     GND    Ground
26   32      I/O/T  GPIO32
27   NC      --     No connection
28   NC      --     No connection
29   33      I/O/T  GPIO33
30   GND     GND    Ground
31   26      I/O/T  GPIO26
32   54      I/O/T  GPIO54
33   48      I/O/T  GPIO48
34   GND     GND    Ground
35   53      I/O/T  GPIO53
36   46      I/O/T  GPIO46
37   47      I/O/T  GPIO47
38   27      I/O/T  GPIO27
39   GND     GND    Ground
40   NC(45)  I/O/T  GPIO45 [2]_
===  ======  =====  ===================================

    P: Power supply;
    I: Input;
    O: Output;
    T: High impedance.

.. [1] GPIO0 and GPIO1 can be enabled by disabling the XTAL_32K function (moving R61/R59 to R199/R197).
.. [2] GPIO45 can be enabled by disabling the SD_PWRn function (moving R231 to R100).

Resources
---------

* `ESP32-P4`_ (Datasheet)
* `ESP32-P4 Series SoC Errata`_
* `ESP32-P4X-Function-EV-Board Reference Design`_ (ZIP)
* `ESP32-P4X-Function-EV-Board User Guide`_
* `ESP32-P4-Function-EV-Board User Guide (v1.5.2)`_ (EOL)
* `ESP32-P4-Function-EV-Board User Guide (v1.4)`_ (EOL)

.. _ESP32-P4: https://www.espressif.com/sites/default/files/documentation/esp32-p4_datasheet_en.pdf
.. _ESP32-P4 Series SoC Errata: https://www.espressif.com/sites/default/files/documentation/esp32-p4_errata_en.pdf
.. _ESP32-P4X-Function-EV-Board: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html
.. _ESP32-P4X-Function-EV-Board Reference Design: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html#related-documents
.. _ESP32-P4X-Function-EV-Board User Guide: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html
.. _ESP32-P4-Function-EV-Board User Guide (v1.5.2): https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html
.. _ESP32-P4-Function-EV-Board User Guide (v1.4): https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide_v1.4.html
