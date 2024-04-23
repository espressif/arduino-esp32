##########################
Frequently Asked Questions
##########################

How to modify an sdkconfig option in Arduino?
---------------------------------------------

Arduino-esp32 project is based on ESP-IDF. While ESP-IDF supports configuration of various compile-time options (known as "Kconfig options" or "sdkconfig options") via a "menuconfig" tool, this feature is not available in Arduino IDE.

To use the arduino-esp32 core with a modified sdkconfig option, you need to use ESP-IDF to compile Arduino libraries. Please see :doc:`esp-idf_component` and :doc:`lib_builder` for the two solutions available.

Note that modifying ``sdkconfig`` or ``sdkconfig.h`` files found in the arduino-esp32 project tree **does not** result in changes to these options. This is because ESP-IDF libraries are included into the arduino-esp32 project tree as pre-built libraries.

How to compile libs with different debug level?
-----------------------------------------------

The short answer is ``esp32-arduino-lib-builder/configs/defconfig.common:44``. A guide explaining the process can be found here <guides/core_debug>
