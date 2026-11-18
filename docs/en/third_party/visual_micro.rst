#######################################
Using Arduino-ESP32 with Visual Micro
#######################################

This guide describes a Microsoft Visual Studio workflow on Windows using
Visual Micro and the Arduino-ESP32 board package.

Visual Micro is a third-party extension with its own licensing and support.
Visual Micro provides support for the IDE integration; Arduino-ESP32 provides
the board definitions, core libraries, and build/upload tools used by the package.

Prerequisites
-------------

* A supported Microsoft Visual Studio installation on Windows and the matching
  Visual Micro extension. Follow the
  `Visual Micro quickstart <https://www.visualmicro.com/page/Quickstart-Arduino-Users.aspx>`_
  for extension installation and initial configuration.
* An ESP32-family board supported by the Arduino-ESP32 release you install.
* A USB data cable and any USB-to-UART driver required by your board.

Install the board package
-------------------------

1. In Visual Studio, open **vMicro > Visual Micro Explorer** and select the
   **Arduino 2** configuration. This configuration can use automatic/default
   locations; an Arduino IDE installation is optional.
2. Open **Board Manager**. Add Espressif's stable package index using
   **Board Support Package Auto-Discovery**, or enter the following URL in the
   additional Board Manager URLs field in **IDE Locations**:

   .. code-block:: text

      https://espressif.github.io/arduino-esp32/package_esp32_index.json

3. Install the **esp32** package by **Espressif Systems**. Record the installed
   version when reproducing a problem or sharing a project configuration.

The package index identifies the available releases; the installed package
supplies the board definitions, compiler, and upload tools. Adding the URL alone
does not install the package or select a board.

If Arduino-ESP32 is already installed through Arduino IDE and both IDEs use the
same default package locations, Visual Micro can use that installation. A
portable solution or a custom package-directory setting can use a different
installation. Check the selected configuration and its locations before
installing a second copy.

See :doc:`../installing` for Arduino-ESP32 installation alternatives and release
index URLs.

Open a sketch and select the target
-----------------------------------

1. Use **File > Open > Arduino Project** to open the main ``.ino`` file of an
   existing sketch. The sketch folder and main ``.ino`` filename must match.
2. With the sketch active, return to **Visual Micro Explorer > Installed**.
   Search **All Boards** for the board and select it for the project.
3. Set the board's available options, such as flash size, PSRAM, partition
   scheme, and USB settings, to match the actual hardware and intended workflow.
   These options depend on the board definition and package version.
4. Connect the board and select its upload port separately.

A development-board name and its underlying ESP32 chip are different things.
If the exact board is absent, first check the package release and the board
vendor's instructions. A generic module definition still needs compatible
flash, PSRAM, pin, and USB settings; a similar name does not establish a match.

Selecting a COM port does not select a board definition. Installing a board
package also does not make an electrically disconnected USB device appear.

Verify build and upload
-----------------------

Start with a small sketch before adding application libraries or configuring a
debugger. For example, save the following as ``EmptySketch/EmptySketch.ino``:

.. code-block:: arduino

   void setup() {
   }

   void loop() {
   }

1. Open the sketch, select the board and its options, choose the **Release**
   solution configuration, and run Visual Micro's **Build** command.
2. If the build succeeds, select the upload port and run
   Visual Micro's **Build & Upload** command.
3. Check the Visual Micro output for the build or upload result. If a board
   changes its USB port during reset, select the appropriate port for the next
   operation.

An upload failure after a successful build should be investigated separately
from package installation. Check the selected port, cable, connector, driver,
and the board vendor's bootloader/reset procedure.

Optional ESP32-S3 native USB debugging
--------------------------------------

After normal build/upload works, ESP32-S3 users can configure a hardware
debugging workflow using its USB Serial/JTAG interface. The board must expose
that interface through the connector being used. A connector wired to a
USB-to-UART bridge provides a different connection path.

The
`Visual Micro ESP32-S3 USB debugging guide <https://www.visualmicro.com/page/ESP32S3-USB-Debugging.aspx#prerequisites>`_
covers the required board configuration, connection, and debugger setup.
Check its prerequisites before applying the procedure to a particular board.

Troubleshooting and support
---------------------------

* **No ESP32 boards listed:** confirm that the package finished installing in
  the configuration and package location currently selected in Visual Micro.
* **Board listed but no upload port:** inspect USB enumeration, the cable,
  connector, and driver; board selection and port discovery are separate.
* **Compilation fails:** retain the first compiler error and the relevant build
  output, together with the core version and selected board options.
* **Upload fails:** retain the upload output and identify the board's connector
  and bootloader/reset procedure.

For Visual Micro integration questions, use Visual Micro's support channels.
When investigating an Arduino-ESP32 core issue, include a minimal sketch, exact
board/core versions, relevant board options, and full diagnostic output. Where
possible, reproduce it using the installation and workflow described in
:doc:`../installing` to distinguish the core issue from IDE integration.
