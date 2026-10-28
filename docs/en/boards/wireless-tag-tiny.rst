########################
Wireless-Tag TINY Boards
########################

The Wireless-Tag TINY family includes compact ESP32-C3, ESP32-C5, ESP32-P4,
and ESP32-S31 development boards. These board definitions provide the default
GPIO assignments, addressable RGB LED, boot button, memory settings, and upload
options. Board-specific display, camera, storage, and wireless functionality
can require additional drivers and hardware configuration.

Board Selection
---------------

.. list-table::
   :header-rows: 1
   :widths: 27 19 13 18 23

   * - Board
     - Target
     - CPU
     - Flash default
     - PSRAM default
   * - WT9932C3-TINY
     - ESP32-C3
     - 160 MHz
     - 4 MB (verify module)
     - Disabled
   * - WT9932C5-TINY
     - ESP32-C5
     - 240 MHz
     - 4 MB (verify module)
     - Disabled
   * - WT9932P4-TINY
     - ESP32-P4 v1.x
     - 360 MHz
     - 16 MB
     - Enabled
   * - WT9932P4X-TINY
     - ESP32-P4 v3.x
     - 400 MHz
     - 16 MB
     - Enabled
   * - WT9932P4C61-TINY
     - ESP32-P4 v3.x
     - 400 MHz
     - 16 MB
     - Enabled
   * - WT9932S31-TINY
     - ESP32-S31
     - 320 MHz
     - 16 MB
     - Enabled (OPI)

The C3 and C5 module memory capacities are not specified by the board support
sources. Their 4 MB setting is a software default, not a claim about every
module fitted to these boards. Check the module marking and detected flash
size, then choose the matching 2, 4, 8, or 16 MB Flash Size option. Each option
selects a matching partition table. PSRAM is not enabled for these boards.

The P4, P4X, P4C61, and S31 entries use the existing 16 MB partition table.
No separate board-manager package or modified core is required by these
definitions.

ESP32-P4 Revisions
------------------

WT9932P4-TINY and WT9932P4X-TINY have similar peripheral wiring but require
different SDK variants. The original P4 board uses ``esp32p4_es`` at 360 MHz;
the P4X board uses ``esp32p4`` at 400 MHz. The vendor identifies the newer
module by an ``X`` suffix on its shield marking. The supported minimum silicon
revision also depends on the SDK bundled with the selected Arduino core.

Select the board that matches the fitted module. A board menu or a compile-time
macro selects the build configuration; it does not detect the silicon revision.
``ESP.getChipRevision()`` reports the runtime silicon revision as
``major * 100 + minor``.

The P4C61 definition targets the newer P4 module. It does not include an
unverified legacy-revision option.

Default Pins
------------

GPIO numbers are used directly. The UART, I2C, and SPI assignments below are
software defaults; they do not imply matching labels on every expansion
connector. Check the board pinout before connecting peripherals.

.. list-table::
   :header-rows: 1

   * - Board
     - RGB / button
     - TX / RX
     - SDA / SCL
     - SS / MOSI / MISO / SCK
   * - C3
     - 6 / 9
     - 21 / 20
     - 4 / 5
     - 7 / 10 / 3 / 2
   * - C5
     - 6 / 28
     - 11 / 12
     - 0 / 1
     - 7 / 8 / 9 / 10
   * - P4 / P4X / P4C61
     - 51 / 35
     - 37 / 38
     - 7 / 8
     - 5 / 32 / 33 / 36
   * - S31
     - 19 / 61
     - 58 / 59
     - 2 / 3
     - 37 / 38 / 39 / 40

The button is active low. ``WT_BUTTON_PIN`` and ``WT_RGB_PIN`` expose its GPIO
and the addressable LED GPIO. ``LED_BUILTIN`` and ``RGB_BUILTIN`` use the core's
virtual RGB pin convention so that the standard Blink sketch works with the
addressable LED. Use ``WT_RGB_PIN`` when an API requires the physical GPIO.

Upload and USB
--------------

The C3 and C5 boards use their USB-to-UART bridge by default, with USB CDC on
boot disabled. Select the bridge's serial port for uploading and monitoring.

The P4 family and S31 default to Hardware CDC and JTAG, with USB CDC on boot
enabled. For the USB-OTG connection, select USB-OTG under USB Mode and USB-OTG
CDC under Upload Mode, and enable USB CDC on boot. Use the connector that
matches the selected mode. Erase All Flash Before Sketch Upload is disabled
by default on every board.

Peripheral Boundaries
---------------------

The P4 and P4X microSD interface uses GPIO43/44 for clock/command and GPIO39-42
for the four data signals. Its GPIO bank uses the 3.3 V VO4 supply (LDO channel
4). The variant provides the corresponding SDMMC and automatic IO-rail
definitions.

The P4C61 card signals are exposed through ``WT_SD_SPI_*`` for use with the
SPI SD library. Its card supply is external; it does not use the P4/P4X
SDMMC power-channel definition. ``WT_HOSTED_*`` records the ESP32-C61 SDIO
wiring only. ESP-Hosted is not enabled by this variant: operation requires a
host configuration and slave firmware that support the actual C61 module.
The P4 board selection does not add a separately programmable C61 target.

The S31 microSD signals use GPIO24/25 for clock/command and GPIO20-23 for data.
Their supply is 1.8 V on LDO channel 1. This variant does not enable generic
SDMMC power defaults for that board. Card operation requires a driver and
power setup that preserve the 1.8 V pad supply; it is outside this board
definition's scope.

Display and camera constants describe board wiring and available connectors.
They do not install display, touch, CSI, DSI, or DVP drivers. Ethernet and
audio definitions from Espressif evaluation boards are not applicable to
these TINY boards.

Hardware References
-------------------

* `Wireless-Tag ESP-IDF BSP board implementations
  <https://github.com/Wireless-TAG-Maker/WT_BSP/tree/916139150e8488b660942078da178b6e76a8f4a1/components/wt_bsp/boards>`_
* `WT9932C3-TINY vendor documentation
  <https://wiki.wireless-tag.com/docs/zh/WT9932C3-TINY/board_features.html>`_
* `WT9932C5-TINY vendor documentation
  <https://wiki.wireless-tag.com/docs/zh/WT9932C5-TINY/board_features.html>`_
* `WT9932P4-TINY and P4X vendor documentation
  <https://wiki.wireless-tag.com/docs/zh/WT9932P4-TINY/board_features.html>`_
* `WT9932P4C61-TINY vendor documentation
  <https://wiki.wireless-tag.com/docs/zh/WT9932P4C61-TINY/board_features.html>`_
* `WT9932S31-TINY vendor documentation
  <https://wiki.wireless-tag.com/docs/zh/WT9932S31-TINY/board_features.html>`_
