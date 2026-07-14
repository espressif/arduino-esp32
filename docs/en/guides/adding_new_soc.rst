################
Adding a New SoC
################

This guide describes every file that must be created or modified to add support for a new Espressif
SoC to the Arduino ESP32 core and the ``esp32-arduino-lib-builder``.

.. contents:: Table of Contents
   :local:
   :depth: 2

Conventions
===========

- ``{soc}`` -- lowercase IDF target name (e.g. ``esp32c5``, ``esp32p4``).
- ``{SOC}`` -- uppercase variant used in C preprocessor macros (e.g. ``ESP32C5``, ``ESP32P4``).
- All ESP-IDF paths are relative to the lib-builder's bundled ESP-IDF
  (``esp32-arduino-lib-builder/esp-idf/``). Always verify against that copy, not a local
  installation on a different branch.
- The most recent completed SoC additions (ESP32-C61 for component-only, ESP32-C5 for full
  support) serve as practical reference diffs.

Support Levels
==============

There are two levels of SoC support. Full support is a strict superset of component support --
you cannot add core support without first completing component support.

Component Support
-----------------

This is the minimum level of component support. The SoC can be used when Arduino is included as an ESP-IDF component
(``idf.py build`` with Arduino in the component list). It involves:

- Handling all ``#error`` guards in core source files so the code compiles for the new target.
- Adding the target to ``idf_component.yml``.
- Basic lib-builder configuration with ``"skip": 1`` in ``builds.json`` (libraries build in CI
  for validation but are not deployed).
- The SoC goes into ``ALL_SOCS``, ``SKIP_LIB_BUILD_SOCS``, and the appropriate
  ``IDF_V*_TARGETS`` arrays in ``socs_config.sh``.

Full Support (Component + Core)
-------------------------------

This level includes everything from component support, plus:

- Prebuilt static libraries ship with the Arduino core package.
- The board appears in Arduino IDE / CLI and can compile and upload sketches directly.
- The SoC is removed from ``SKIP_LIB_BUILD_SOCS`` and added to ``CORE_VARIANTS``.
- Added to CI test targets, package index, PlatformIO build script, etc.

Throughout this guide, each step is marked **[component]** (required for both levels) or
**[core]** (only required for full support).

Prerequisites
=============

Before starting, ensure you have access to:

1. The lib-builder's bundled ESP-IDF with the new SoC already supported by IDF (the target
   must exist in ``components/soc/{soc}/``).
2. The DevKit schematic for the new SoC (from the
   `Espressif DevKits page <https://docs.espressif.com/projects/esp-dev-kits/>`_).
3. Confirmation that ``esptool`` supports the new chip (check
   `esptool releases <https://github.com/espressif/esptool/releases>`_ for the ROM class at
   ``esptool/targets/{soc}.py``).
4. The ``openocd-esp32`` package includes a debug config (``board/{soc}-builtin.cfg``) if
   debugging support is needed at launch.

Step 1: Gather SoC Information from ESP-IDF [component]
=======================================================

Collect hardware parameters from the lib-builder's bundled ESP-IDF. The table below lists what
you need and where to find it. All paths use the convention ``components/soc/{soc}/...`` unless
stated otherwise.

.. list-table::
   :widths: 25 45 30
   :header-rows: 1

   * - Information
     - ESP-IDF Path
     - Key Symbols / Notes
   * - Peripheral capabilities
     - ``components/soc/{soc}/include/soc/soc_caps.h``
     - ``SOC_WIFI_SUPPORTED``, ``SOC_USB_OTG_SUPPORTED``, ``SOC_BT_SUPPORTED``,
       ``SOC_TOUCH_SENSOR_SUPPORTED``, ``SOC_GPIO_PIN_COUNT``, ``SOC_IEEE802154_SUPPORTED``, etc.
   * - CPU / XTAL frequencies
     - ``components/soc/{soc}/include/soc/clk_tree_defs.h``
     - ``SOC_CLK_XTAL_FREQ_*``, CPU clock source enums, max CPU frequency.
   * - UART default pins
     - ``components/soc/{soc}/include/soc/uart_pins.h``
     - ``U0TXD_GPIO_NUM``, ``U0RXD_GPIO_NUM``, ``U1TXD_GPIO_NUM``, etc.
   * - Boot strapping GPIO
     - ``components/soc/{soc}/include/soc/boot_mode.h`` and DevKit schematic
     - Not all SoCs have ``boot_mode.h``. Cross-reference with the DevKit schematic to find
       the BOOT button GPIO.
   * - Bootloader flash offset
     - ``components/bootloader/Kconfig.projbuild``
     - ``CONFIG_BOOTLOADER_OFFSET_IN_FLASH``: per-SoC defaults.
   * - Flash frequency options
     - ``components/spi_flash/{soc}/Kconfig.flash_freq``
     - ``ESPTOOLPY_FLASHFREQ_*`` choices and defaults. Verify against the lib-builder's IDF.
   * - Flash frequency encoding
     - ``components/esptool_py/Kconfig.projbuild``
     - Maps Kconfig choices to esptool strings. Also check
       ``esptool/targets/{soc}.py`` for the byte encoding dictionary.
   * - eFuse registers (MAC address)
     - ``components/soc/{soc}/register/soc/efuse_reg.h``
     - Register naming varies by SoC (e.g. ``EFUSE_RD_MAC_SPI_SYS_*_REG``,
       ``EFUSE_RD_MAC_SYS0_REG``).
   * - eFuse registers (package version)
     - ``components/soc/{soc}/register/soc/efuse_reg.h``
     - ``EFUSE_PKG_VERSION`` field and its containing register.
   * - GPIO device structure
     - ``components/soc/{soc}/register/soc/gpio_struct.h``
     - Check if pin access is ``pin[n].int_type`` or ``pinn[n].pinn_int_type``
       (newer SoCs use the ``pinn`` variant).
   * - SPI peripheral base address
     - ``components/soc/{soc}/include/soc/reg_base.h``
     - ``DR_REG_SPI2_BASE`` or ``DR_REG_GPSPI2_BASE`` (newer chips use ``GPSPI`` naming).
   * - USB Serial/JTAG registers
     - ``components/soc/{soc}/register/soc/usb_serial_jtag_struct.h``
     - Only if ``SOC_USB_SERIAL_JTAG_SUPPORTED``. Check the IDF HAL layer
       (``usb_serial_jtag_ll.h``) for abstracted register access.
   * - USB D+/D- pins
     - ``components/soc/{soc}/include/soc/usb_pins.h``
     - Only for SoCs with routable USB PHY (``SOC_USB_OTG_SUPPORTED``).
   * - ADC channel to GPIO mapping
     - ``components/soc/{soc}/include/soc/adc_channel.h``
     - ``ADC1_CHANNEL_0_GPIO_NUM``, etc.
   * - Touch pad to GPIO mapping
     - ``components/soc/{soc}/include/soc/touch_sensor_channel.h``
     - Only if ``SOC_TOUCH_SENSOR_SUPPORTED``.
   * - SPI default IOMUX pins
     - ``components/esp_hal_gpspi/{soc}/include/soc/spi_pins.h``
     - ``SPI2_IOMUX_PIN_NUM_CLK``, ``SPI2_IOMUX_PIN_NUM_MOSI``, etc.
   * - Chip model / ID
     - ``components/esp_hw_support/include/esp_chip_info.h``
     - ``esp_chip_model_t`` enum.
   * - OpenOCD debug config
     - **External:** ``openocd-esp32`` package
     - Config files under ``board/{soc}-builtin.cfg``.
   * - DevKit schematics
     - `Espressif DevKits page <https://docs.espressif.com/projects/esp-dev-kits/>`_
     - Pin assignments, LED GPIOs, USB connector wiring, BOOT/RESET button GPIOs.

.. note::

   For SoCs with hardware versioning (e.g. ESP32-P4 with ``hw_ver1``/``hw_ver3``), register
   headers may be under ``components/soc/{soc}/register/hw_ver*/soc/`` instead of the usual
   ``components/soc/{soc}/register/soc/``.

Step 2: Lib-Builder Changes
============================

The ``esp32-arduino-lib-builder`` builds the precompiled static libraries that ship with the
Arduino core. These changes should be done first because the Arduino core depends on the
libraries the builder produces.

2.1 Build Target Definition [component]
----------------------------------------

**File:** ``configs/builds.json``

Add a new object to the ``targets`` array. For component-only support, include ``"skip": 1``
so the target is excluded from production deploys but still built in CI for validation.

Each value in ``features``, ``idf_libs``, ``bootloaders``, and ``mem_variants`` corresponds
to a defconfig fragment filename under ``configs/`` (e.g. ``"80m"`` means
``configs/defconfig.80m``).

.. code-block:: json

   {
     "target": "{soc}",
     "skip": 1,
     "features": [],
     "idf_libs": ["qio", "80m"],
     "bootloaders": [
       ["qio", "80m"],
       ["dio", "80m"],
       ["qio", "40m"],
       ["dio", "40m"]
     ],
     "mem_variants": [
       ["dio", "80m"]
     ]
   }

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Field
     - Description
   * - ``target``
     - The IDF target name (passed to ``idf.py -DIDF_TARGET=``).
   * - ``chip_variant``
     - Optional. Output directory name when it differs from ``target`` (e.g. ``esp32p4_es``
       for an early-silicon variant). Defaults to ``target``.
   * - ``skip``
     - Set to ``1`` for component-only support. Remove when promoting to full support.
   * - ``features``
     - Defconfig fragments applied to all builds. Maps to ``configs/defconfig.<name>``.
   * - ``idf_libs``
     - Defconfig fragments for the main static library build (typically flash mode + freq).
   * - ``bootloaders``
     - List of ``[mode, freq]`` tuples. Each produces a bootloader ELF. Include all
       combinations users can select in ``boards.txt``.
   * - ``mem_variants``
     - List of ``[mode, freq]`` tuples for alternative memory configs (e.g. DIO fallback).

Also add the new SoC to the ``mem_variants_files`` section at the top of the file -- at minimum
to the ``libspi_flash.a`` entry's ``targets`` array.

**To promote to full support [core]:** Remove ``"skip": 1`` from the target entry.

2.2 SoC Defconfig [component]
------------------------------

**File:** ``configs/defconfig.{soc}``

Create a per-SoC Kconfig fragment. The build system chains configs as:
``defconfig.common`` + ``defconfig.{soc}`` + feature/mode/freq fragments.

Use the closest existing SoC as a template. Common options to set:

- Bluetooth stack (``CONFIG_BT_NIMBLE_ENABLED``, ``CONFIG_BT_BLUEDROID_ENABLED``)
- PSRAM (``CONFIG_SPIRAM=y``) if supported
- OpenThread / IEEE 802.15.4 settings if ``SOC_IEEE802154_SUPPORTED``
- Wi-Fi settings if ``SOC_WIFI_SUPPORTED``
- Zigbee: set ``CONFIG_ZB_ENABLED=n`` if prebuilt Zigbee libraries are not available yet
- TinyUSB / USB settings if ``SOC_USB_OTG_SUPPORTED``
- CPU frequency default

2.3 Flash Frequency Fragments [component]
-------------------------------------------

**Files:** ``configs/defconfig.{freq}`` and ``main/Kconfig.projbuild``

If the SoC introduces flash frequencies not already covered by existing fragments, create
new files (e.g. ``configs/defconfig.48m``):

.. code-block:: text

   CONFIG_ESPTOOLPY_FLASHFREQ_{FREQ}M=y

And add the mapping in ``main/Kconfig.projbuild`` under ``LIB_BUILDER_FLASHFREQ``:

.. code-block:: text

   default "{freq}m" if ESPTOOLPY_FLASHFREQ_{FREQ}M

The frequency values in ``builds.json`` must match an available ``ESPTOOLPY_FLASHFREQ_*``
Kconfig choice in the lib-builder's bundled IDF. If they don't match, the option is silently
ignored and the build produces files with unexpected names.

2.4 TinyUSB Component [component, if applicable]
--------------------------------------------------

**Files:** ``components/arduino_tinyusb/CMakeLists.txt``,
``components/arduino_tinyusb/Kconfig.projbuild``

Only if ``SOC_USB_OTG_SUPPORTED``:

- Add an ``IDF_TARGET`` branch with the correct ``OPT_MCU_ESP32*`` define.
- Add ``depends on IDF_TARGET_{SOC}`` to the Kconfig.

2.5 Arduino Component CMakeLists [component]
----------------------------------------------

During the lib-builder build, the Arduino core is cloned/linked into
``components/arduino/`` (this directory is gitignored). The ``CMakeLists.txt`` used is the
**same file** as the Arduino core's root ``CMakeLists.txt`` (see Step 3.8). No separate
file exists in the lib-builder repo -- just ensure the core's ``CMakeLists.txt`` has correct
``IDF_TARGET`` conditionals before building.

2.6 Managed Component Dependencies [component]
------------------------------------------------

**File:** ``main/idf_component.yml``

Update dependency ``rules:`` to include or exclude the new SoC for components like
``esp32-camera``, ``esp-tflite-micro``, ``esp-sr``, ``esp_matter``, etc., based on the
SoC's capabilities.

2.7 Copy-Libs Script [component, if applicable]
-------------------------------------------------

**File:** ``tools/copy-libs.sh``

If the new SoC depends on headers from another SoC's Bluetooth or other component at build
time (e.g. ESP32-C61 shares BT headers with ESP32-C6), add the appropriate cross-SoC copy
logic.

2.8 Toolchain Configuration [component]
-----------------------------------------

**File:** ``tools/get_projbuild_gitconfig.py``

This file has a hardcoded if-chain for architecture detection. If the new SoC uses RISC-V,
add it to the condition that sets ``arch_target = "riscv32-esp"``. If Xtensa, add it to the
Xtensa branch.

2.9 CI Build Workflow [component]
----------------------------------

**File:** ``.github/workflows/push.yml``

Add the new SoC (using ``chip_variant`` if applicable) to the ``matrix.target`` array.
This ensures the target is built in CI for every push, even with ``"skip": 1``.

2.10 CI Deploy Workflow [core]
-------------------------------

**File:** ``.github/workflows/cron.yml``

Add the SoC to the ``targets:`` string in the active matrix entry. This enables automatic
deployment of prebuilt libraries.

2.11 PlatformIO Template [core, if applicable]
------------------------------------------------

**File:** ``configs/pioarduino_start.txt``

If the SoC supports Zigbee (``SOC_IEEE802154_SUPPORTED``), add it to the appropriate
``build_mcu in [...]`` list for native 802.15.4 Zigbee library selection.

Step 3: Arduino Core Changes
=============================

3.1 SoC Configuration Script [component]
------------------------------------------

**File:** ``.github/scripts/socs_config.sh``

This is the central SoC registry. Add the new SoC to:

- ``ALL_SOCS``: always.
- ``SKIP_LIB_BUILD_SOCS``: for component-only support.
- ``IDF_V*_TARGETS``: add to every IDF version array that supports this chip.

If the SoC uses Xtensa, add it to the explicit case in ``get_arch()``. RISC-V SoCs are
handled by the default case.

**To promote to full support [core]:**

- Remove from ``SKIP_LIB_BUILD_SOCS``.
- Add to ``CORE_VARIANTS``.
- Add to ``HW_TEST_TARGETS`` when hardware CI runners are available.
- Add to ``WOKWI_TEST_TARGETS`` when Wokwi simulation support exists.
- Add to ``QEMU_TEST_TARGETS`` when QEMU support exists.

3.2 Board Definition [component]
----------------------------------

**File:** ``boards.txt``

Add a Dev Module entry block. Use an existing similar SoC as a template (e.g. a RISC-V chip
with similar peripherals). The board entry is required even for component-only support because
the IDF component build uses ``ARDUINO_VARIANT`` to locate pin definitions.

Key properties to set (source of truth noted for each):

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Property
     - Source
   * - ``build.tarch``
     - ``riscv32`` or ``xtensa`` (from SoC architecture)
   * - ``build.mcu``
     - The IDF target name
   * - ``build.variant``
     - Directory name under ``variants/``
   * - ``build.f_cpu``
     - Max CPU frequency in Hz (from ``clk_tree_defs.h``)
   * - ``build.bootloader_addr``
     - From ``CONFIG_BOOTLOADER_OFFSET_IN_FLASH`` in IDF's ``Kconfig.projbuild``
   * - ``build.flash_freq``
     - Default flash frequency string (from ``spi_flash/{soc}/Kconfig.flash_freq``)
   * - ``build.img_freq``
     - Frequency written to image header. This may differ from ``flash_freq`` for SoCs with
       non-40 MHz XTALs (check ``esptool_py/Kconfig.projbuild`` for mapping)
   * - ``build.usb_mode``
     - ``0`` for USB-OTG (TinyUSB), ``1`` for HW CDC/JTAG. Only if USB supported.
   * - ``upload.maximum_size``
     - Flash size minus overhead (from default partition table)
   * - ``upload.maximum_data_size``
     - SRAM size (from datasheet or ``soc_caps.h``)

Menu entries to add as applicable (based on ``soc_caps.h``):

- ``CPUFreq``, ``FlashFreq``, ``FlashMode``, ``FlashSize``, ``PartitionScheme``
- ``UploadSpeed``, ``DebugLevel``, ``EraseFlash``, ``JTAGAdapter``
- ``USBMode`` (if ``SOC_USB_OTG_SUPPORTED``)
- ``CDCOnBoot`` (if ``SOC_USB_SERIAL_JTAG_SUPPORTED`` or ``SOC_USB_OTG_SUPPORTED``)
- ``ZigbeeMode`` (if ``SOC_IEEE802154_SUPPORTED``)
- ``PSRAM`` (if ``SOC_SPIRAM_SUPPORTED``)

For component-only support, the board entry still exists but is not usable from the Arduino
IDE without prebuilt libraries.

3.3 Variant [component]
-------------------------

**Directory:** ``variants/{soc}/``

Create ``variants/{soc}/pins_arduino.h`` with default pin definitions:

- ``TX`` / ``RX``: Default UART0 pins (from ``uart_pins.h``)
- ``SDA`` / ``SCL``: Default I2C pins (from DevKit schematic)
- ``SS`` / ``MOSI`` / ``MISO`` / ``SCK``: Default SPI pins (from ``spi_pins.h``)
- ``LED_BUILTIN``: Onboard LED GPIO (from DevKit schematic)
- Analog pin macros (``A0``, ``A1``, ...): ADC GPIO mappings (from ``adc_channel.h``)

3.4 Platform Configuration [component]
----------------------------------------

**File:** ``platform.txt``

Add:

1. **Build extra flags** (in the target-dependent section):

   .. code-block:: text

      build.extra_flags.{soc}=...

   The flags depend on USB capabilities:

   - **No USB at all**: ``-DARDUINO_USB_CDC_ON_BOOT=0``
   - **USB Serial/JTAG only**: ``-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT={build.cdc_on_boot}``
   - **USB-OTG + Serial/JTAG**: ``-DARDUINO_USB_MODE={build.usb_mode} -DARDUINO_USB_CDC_ON_BOOT={build.cdc_on_boot} -DARDUINO_USB_MSC_ON_BOOT={build.msc_on_boot} -DARDUINO_USB_DFU_ON_BOOT={build.dfu_on_boot}``

2. **Debug configuration** (in the debugger section):

   .. code-block:: text

      debug_script.{soc}={soc}-builtin.cfg
      debug_config.{soc}=

   Newer SoCs use this minimal form. Older SoCs (ESP32, S2, S3, C3) have full
   ``cortex-debug.custom.*`` blocks -- use those as a reference only if the OpenOCD config
   requires special attach/restart commands.

3.5 Core Source Files with #error Guards [component]
-----------------------------------------------------

These files contain ``#error`` directives and **will not compile** without adding a branch
for the new SoC:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - File
     - What to add
   * - ``cores/esp32/esp32-hal.h``
     - ``BOOT_PIN`` definition (from DevKit schematic / boot_mode.h)
   * - ``cores/esp32/esp32-hal-cpu.c``
     - ROM RTC header include and clock source names array
   * - ``cores/esp32/esp32-hal-misc.c``
     - ROM RTC header include
   * - ``cores/esp32/esp32-hal-matrix.c``
     - GPIO signal count include
   * - ``cores/esp32/esp32-hal-spi.c``
     - SPI bus array with base address (``DR_REG_SPI2_BASE`` vs ``DR_REG_GPSPI2_BASE``)
   * - ``cores/esp32/esp32-hal-psram.c``
     - PSRAM type include. Only if ``SOC_SPIRAM_SUPPORTED``; the ``#error`` fires for
       SoCs that support PSRAM but are not yet handled.
   * - ``cores/esp32/Esp.cpp``
     - ``ESP_FLASH_IMAGE_BASE`` (same as ``build.bootloader_addr``) and chip model string

3.6 Core Source Files That Need Review [component]
--------------------------------------------------

These files may compile without changes but can produce incorrect behavior if the new SoC
has different peripheral layouts:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - File
     - What to check
   * - ``cores/esp32/HardwareSerial.h``
     - UART port count and default pins (from ``soc_caps.h``, ``uart_pins.h``)
   * - ``cores/esp32/esp32-hal-gpio.c``
     - ``gpio_dev_t`` pin access pattern: ``pin[n].int_type`` vs ``pinn[n].pinn_int_type``
       (from ``gpio_struct.h``)
   * - ``cores/esp32/esp32-hal-uart.c``
     - UART clock sources and hardware limits
   * - ``cores/esp32/esp32-hal-i2c-slave.c``
     - I2C slave driver port count and config
   * - ``cores/esp32/esp32-hal-ledc.c``
     - LEDC timer/channel count, clock sources
   * - ``cores/esp32/esp32-hal-cpu.h``
     - CPU frequency table and validation
   * - ``cores/esp32/chip-debug-report.cpp``
     - Package version eFuse register (from ``efuse_reg.h``)
   * - ``cores/esp32/esp32-hal-tinyusb.c``
     - eFuse MAC register names. Only if ``SOC_USB_OTG_SUPPORTED``.
   * - ``cores/esp32/HWCDC.cpp``
     - USB Serial/JTAG PHY configuration. Only if ``SOC_USB_SERIAL_JTAG_SUPPORTED``.
       The IDF HAL layer (``usb_serial_jtag_ll_*``) handles most abstraction, but the
       PHY init section accesses registers directly.
   * - ``cores/esp32/esp32-hal-adc.c``
     - ADC channel count and configuration
   * - ``cores/esp32/esp32-hal-touch.c`` / ``esp32-hal-touch-ng.c``
     - Only if ``SOC_TOUCH_SENSOR_SUPPORTED``
   * - ``cores/esp32/USB.cpp``
     - Only if ``SOC_USB_OTG_SUPPORTED``

3.7 Library Source Files [component]
-------------------------------------

- **``libraries/SPI/src/SPI.cpp``**: Almost always needs updating for SPI pin/bus
  configuration differences.
- **Library ``ci.yml`` files**: Update target lists or requirement conditions
  in example directories to include or exclude the new SoC based on capabilities.

3.8 CMakeLists.txt (IDF Component) [component]
------------------------------------------------

**File:** ``CMakeLists.txt`` (repository root)

Review and update ``IDF_TARGET`` conditionals:

- **OpenThread**: Add to the ``requires openthread`` condition if
  ``SOC_IEEE802154_SUPPORTED``.
- **Touch sensor**: Add to the ``requires esp_driver_touch_sens`` condition if
  ``SOC_TOUCH_SENSOR_SUPPORTED``.
- **TinyUSB**: Add to the ``IDF_TARGET MATCHES`` condition for ``arduino_tinyusb`` if
  ``SOC_USB_OTG_SUPPORTED``.

3.9 IDF Component Metadata [component]
----------------------------------------

**File:** ``idf_component.yml``

- Add the new SoC to the ``targets:`` list.
- Add ``variants/{soc}/**/*`` to the ``files.include`` section.
- Update dependency ``rules:`` conditions (e.g. ``esp_matter``, ``esp32-camera``, ``esp-sr``)
  to include or exclude the new SoC based on capabilities.

3.10 PlatformIO Build Script [core]
-------------------------------------

**File:** ``tools/pioarduino-build.py``

Add the new SoC to the correct bootloader offset group. The offset comes from
``CONFIG_BOOTLOADER_OFFSET_IN_FLASH`` in ESP-IDF's ``components/bootloader/Kconfig.projbuild``.

.. note::

   There is also a per-SoC ``pioarduino-build.py`` inside
   ``tools/esp32-arduino-libs/{soc}/``. That file is **auto-generated** by the lib-builder's
   ``tools/copy-libs.sh`` from ``configs/pioarduino_start.txt`` + build output +
   ``configs/pioarduino_end.txt``. No manual action is needed for that file.

3.11 Debug SVD File [core]
----------------------------

**File:** ``tools/ide-debug/svd/{soc}.svd``

Add the SVD (System View Description) file if available from Espressif. This enables
register-level debugging in IDEs. Not all SoCs have SVD files available at launch.

3.12 Package Index [core]
---------------------------

**File:** ``package/package_esp32_index.template.json``

Add a board entry (e.g. ``"name": "ESP32-XX Dev Board"``). Also verify that the pinned
``esptool_py`` and ``openocd-esp32`` versions support the new SoC:

- **esptool**: Must include the chip's ROM class (``esptool/targets/{soc}.py``). If the
  pinned version is too old, update the version and all per-platform URLs/checksums.
- **OpenOCD**: Must include ``board/{soc}-builtin.cfg``.
- **Toolchains**: Auto-updated by ``gen_tools_json.py`` in the lib-builder from IDF's
  ``tools/tools.json`` -- typically no manual changes needed.

3.13 CI Defaults [core]
-------------------------

**File:** ``.github/scripts/sketch_utils.sh``

Add default FQBN options for the new SoC in the ``default_fqbn_for_target()`` function.
Define a ``{soc}_opts`` variable with appropriate defaults (e.g. PSRAM enabled, USB mode).

3.14 Example Gating [component]
---------------------------------

Examples that don't work on the new SoC (e.g. Wi-Fi examples on a chip without Wi-Fi) need to be
excluded. The modern approach uses ``ci.yml`` files in example directories
with explicit ``targets`` or ``requires`` fields.

Check existing examples and add exclusions where the SoC lacks required capabilities.

3.15 Test Pin Configuration [core]
------------------------------------

**File:** ``tests/validation/adc_pwm/pins_config.h`` (and similar test files)

Add a ``#elif CONFIG_IDF_TARGET_{SOC}`` block with appropriate test GPIO assignments.
Files with ``#error "add pins for this target"`` guards will indicate where this is needed.

3.16 CI Workflows [component/core]
------------------------------------

**[component]:**

- ``.github/workflows/build_component.yml``: Ensure the IDF version matrix covers the new
  target (driven by ``socs_config.sh``'s ``IDF_V*_TARGETS``).

**[core]:**

- Test workflows (``tests.yml``, ``tests_hw_wokwi.yml``): Driven by ``socs_config.sh``
  test target arrays.
- Release workflow (``.github/workflows/release.yml``): May reference target lists for
  packaging.

3.17 Documentation [component]
-------------------------------

Several documentation files reference SoC-specific information and should be updated:

- **``docs/en/boards/boards.rst``**: Add the new SoC to the family list with its key
  features (connectivity, architecture). For component-only SoCs, add a footnote indicating
  that rebuilding static libraries or using Arduino as an IDF component is required.

- **``docs/en/libraries.rst``**: Update the peripheral support matrix table. Add a column
  for the new SoC with ``|yes|``, ``|no|``, or ``|n/a|`` for each feature based on
  ``soc_caps.h``.

- **``docs/en/contributing.rst``**: Update the default FQBN list if the SoC has full
  support (this list mirrors what ``sketch_utils.sh`` defines).

- **``.github/CI_README.md``**:

  - The "Adding a New SoC" section documents the CI-specific steps (updating
    ``socs_config.sh``). Verify it is still accurate after your changes.
  - The "DevKit GPIO Reference" section: Add a GPIO reference table for the new SoC's
    DevKit board, documenting broken-out pins, strapping pins, peripheral assignments,
    and safe pin categories.

- **Board-specific RST** (optional): If a dedicated documentation page is needed for the
  DevKit, add a file under ``docs/en/boards/`` and include it in the ``boards.rst`` toctree.

3.18 Validation Tests [core]
------------------------------

The ``tests/validation/`` directory contains hardware validation tests. For full support,
ensure the new SoC can run the relevant validation tests:

- Tests with ``pins_config.h`` files that have ``#error`` guards must have pin assignments
  added (see Step 3.15).
- Tests with ``ci.yml`` files that specify ``targets:`` lists should include the new SoC
  where applicable.
- At minimum, verify that these validation test categories pass on hardware:
  ``gpio``, ``uart``, ``spi``, ``i2c_master``, ``adc_pwm``, ``timer``, ``periman``.
- For SoCs with specific capabilities, also run: ``ble``, ``wifi``, ``touch``, ``psram``,
  ``openthread``, ``zigbee``, ``ethernet``, as applicable.

Step 4: Verification
=====================

Component Support Checklist
----------------------------

.. code-block:: text

   [ ] configs/builds.json has the target entry (with "skip": 1)
   [ ] configs/defconfig.{soc} exists with appropriate options
   [ ] ./build.sh -t {soc} completes successfully in the lib-builder
   [ ] All #error guards in cores/esp32/ are resolved
   [ ] idf_component.yml includes the SoC in targets list
   [ ] CMakeLists.txt conditionals are updated
   [ ] boards.txt has a Dev Module entry
   [ ] variants/{soc}/pins_arduino.h exists
   [ ] platform.txt has build.extra_flags.{soc} and debug_script.{soc}
   [ ] socs_config.sh has the SoC in ALL_SOCS, SKIP_LIB_BUILD_SOCS, and IDF_V*_TARGETS
   [ ] Lib-builder push.yml includes the target in CI matrix
   [ ] Arduino build_component.yml covers the SoC
   [ ] docs/en/boards/boards.rst updated with new SoC family entry
   [ ] docs/en/libraries.rst support matrix updated
   [ ] A basic sketch (Blink) compiles when using Arduino as IDF component
   [ ] CI passes on both repositories

Full Support Checklist (in addition to above)
----------------------------------------------

.. code-block:: text

   [ ] builds.json: "skip" removed
   [ ] cron.yml: target added to deploy matrix
   [ ] socs_config.sh: SoC in CORE_VARIANTS, removed from SKIP_LIB_BUILD_SOCS
   [ ] sketch_utils.sh: default FQBN options defined
   [ ] pioarduino-build.py: bootloader offset group updated
   [ ] package_esp32_index.template.json: board entry added, tool versions verified
   [ ] Bootloader ELF files generated with correct names
   [ ] Libraries built in <lib-builder>/out/tools/esp32-arduino-libs/{soc}/ and copied to <arduino-repo>/tools/esp32-arduino-libs/{soc}/
   [ ] .github/CI_README.md: DevKit GPIO reference table added
   [ ] Sketch compiles and uploads via Arduino IDE/CLI
   [ ] USB Serial/JTAG (if applicable) works for upload and monitor
   [ ] Validation tests pass: gpio, uart, spi, i2c_master, adc_pwm, timer, periman
   [ ] Capability-specific tests pass (ble, wifi, touch, psram, etc. as applicable)
   [ ] CI passes on both repositories

Appendix A: Flash Frequency Reference
=======================================

Flash frequency configuration is error-prone due to indirection between Kconfig names,
esptool encoding values, and actual flash speeds.

For SoCs with a **40 MHz XTAL**, the Kconfig option names match the actual flash speeds
and ``flash_freq`` / ``img_freq`` / ``boot_freq`` are all the same (e.g. ``80m`` everywhere).

For SoCs with a **32 MHz XTAL** (or other non-40 MHz crystals), there is additional indirection:

1. **Kconfig option name** (e.g. ``ESPTOOLPY_FLASHFREQ_64M``): The label defined in
   ``spi_flash/{soc}/Kconfig.flash_freq``.

2. **esptool string** (e.g. ``48m``): The value of ``CONFIG_ESPTOOLPY_FLASHFREQ`` after
   Kconfig resolution (defined in ``esptool_py/Kconfig.projbuild``). Passed to esptool's
   ``--flash-freq`` argument.

3. **esptool encoding** (e.g. ``0xF``): The byte value written to the flash image header.
   Defined in ``esptool/targets/{soc}.py`` in the ``FLASH_FREQUENCY`` dictionary.

4. **Bootloader filename**: ``bootloader_{boot}_{boot_freq}.elf`` where ``boot_freq``
   defaults to ``flash_freq`` (from ``platform.txt``).

5. **Lib-builder filename**: Determined by ``LIB_BUILDER_FLASHFREQ`` in
   ``main/Kconfig.projbuild``, which maps the Kconfig choice to the output directory string.

The critical rule: the frequency names in ``builds.json`` must match an available
``ESPTOOLPY_FLASHFREQ_*`` Kconfig choice in the lib-builder's bundled IDF. If they don't,
the defconfig option is silently ignored, the build falls through to the Kconfig default,
and bootloader files end up with unexpected names.

Appendix B: File Reference Summary
====================================

.. list-table::
   :widths: 50 15 35
   :header-rows: 1

   * - Arduino Core File
     - Level
     - Purpose
   * - ``.github/scripts/socs_config.sh``
     - component
     - Central SoC registry
   * - ``.github/scripts/sketch_utils.sh``
     - core
     - Default FQBN options per target
   * - ``boards.txt``
     - component
     - Board definitions and menu options
   * - ``platform.txt``
     - component
     - Build flags, debug config
   * - ``variants/{soc}/pins_arduino.h``
     - component
     - Default pin assignments
   * - ``CMakeLists.txt``
     - component
     - IDF component dependencies
   * - ``idf_component.yml``
     - component
     - IDF component metadata and target list
   * - ``cores/esp32/esp32-hal.h``
     - component
     - ``BOOT_PIN`` definition
   * - ``cores/esp32/esp32-hal-cpu.c``
     - component
     - Clock source names, ROM header
   * - ``cores/esp32/esp32-hal-misc.c``
     - component
     - ROM header include
   * - ``cores/esp32/esp32-hal-matrix.c``
     - component
     - GPIO signal count
   * - ``cores/esp32/esp32-hal-spi.c``
     - component
     - SPI bus base addresses
   * - ``cores/esp32/esp32-hal-psram.c``
     - component
     - PSRAM type detection
   * - ``cores/esp32/Esp.cpp``
     - component
     - Flash image base, chip model strings
   * - ``cores/esp32/HardwareSerial.h``
     - component
     - UART port/pin definitions
   * - ``cores/esp32/esp32-hal-gpio.c``
     - component
     - GPIO HAL pin structure access
   * - ``cores/esp32/esp32-hal-i2c-slave.c``
     - component
     - I2C slave driver config
   * - ``cores/esp32/chip-debug-report.cpp``
     - component
     - Package version eFuse
   * - ``cores/esp32/esp32-hal-tinyusb.c``
     - component
     - TinyUSB MAC address (if USB-OTG)
   * - ``cores/esp32/HWCDC.cpp``
     - component
     - USB Serial/JTAG PHY (if applicable)
   * - ``libraries/SPI/src/SPI.cpp``
     - component
     - SPI pin/bus configuration
   * - ``tools/pioarduino-build.py``
     - core
     - PlatformIO bootloader offset
   * - ``tools/ide-debug/svd/{soc}.svd``
     - core
     - Debug SVD file
   * - ``package/package_esp32_index.template.json``
     - core
     - Board Manager package index
   * - ``tests/validation/*/pins_config.h``
     - core
     - Test pin assignments
   * - ``docs/en/boards/boards.rst``
     - component
     - SoC family list and descriptions
   * - ``docs/en/libraries.rst``
     - component
     - Peripheral support matrix per SoC
   * - ``docs/en/contributing.rst``
     - core
     - Default FQBN list for CI
   * - ``.github/CI_README.md``
     - core
     - DevKit GPIO reference tables, CI SoC guide

.. list-table::
   :widths: 50 15 35
   :header-rows: 1

   * - Lib-Builder File
     - Level
     - Purpose
   * - ``configs/builds.json``
     - component
     - Build target matrix
   * - ``configs/defconfig.{soc}``
     - component
     - SoC-specific Kconfig fragment
   * - ``configs/defconfig.common``
     - --
     - Shared base config (rarely needs changes)
   * - ``configs/defconfig.{freq}``
     - component
     - Flash frequency Kconfig fragment
   * - ``main/Kconfig.projbuild``
     - component
     - Flash mode/frequency string mappings
   * - ``main/idf_component.yml``
     - component
     - Managed component dependencies
   * - ``components/arduino_tinyusb/``
     - component
     - TinyUSB component (if USB-OTG)
   * - ``tools/copy-libs.sh``
     - component
     - Library copy logic (cross-SoC headers)
   * - ``tools/get_projbuild_gitconfig.py``
     - component
     - Toolchain arch detection
   * - ``configs/pioarduino_start.txt``
     - core
     - PlatformIO Zigbee library template
   * - ``.github/workflows/push.yml``
     - component
     - CI build matrix
   * - ``.github/workflows/cron.yml``
     - core
     - Deploy targets
