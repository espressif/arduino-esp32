Compatibility Guide for ESP32 Arduino Core
==========================================

Introduction
------------

Welcome to the compatibility guide for library developers aiming to support multiple versions of the ESP32 Arduino core. This documentation provides essential tips and best practices for ensuring compatibility with 2.x, 3.x and future versions of the ESP32 Arduino core.

Code Adaptations
----------------

To ensure compatibility with both versions of the ESP32 Arduino core, developers should utilize conditional compilation directives in their code. Below is an example of how to conditionally include code based on the ESP32 Arduino core version::

    .. code-block:: cpp

      #ifdef ESP_ARDUINO_VERSION_MAJOR
      #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
          // Code for version 3.x
      #else
          // Code for version 2.x
      #endif
      #else
          // Code for version 1.x
      #endif

Version Print
-------------

To easily print the ESP32 Arduino core version at runtime, developers can use the `ESP_ARDUINO_VERSION_STR` macro. Below is an example of how to print the ESP32 Arduino core version::

    .. code-block:: cpp

      Serial.printf(" ESP32 Arduino core version: %s\n", ESP_ARDUINO_VERSION_STR);

API Differences
---------------

Developers should be aware, that there may be API differences between major versions of the ESP32 Arduino core. For this we created a `Migration guide <https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides.html>`_. to help developers transition from between major versions of the ESP32 Arduino core.

Library Testing
---------------

We have added an External Library Test CI job, which tests external libraries with the latest version of the ESP32 Arduino core to help developers ensure compatibility with the latest version of the ESP32 Arduino core.
If you want to include your library in the External Library Test CI job, please follow the instructions in the `External Libraries Test <https://docs.espressif.com/projects/arduino-esp32/en/latest/esp32/external_libraries_test.html>`_.
