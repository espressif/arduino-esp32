###############################
Arduino as an ESP-IDF component
###############################

About
-----

You can use the Arduino framework as an ESP-IDF component. This allows you to use the Arduino framework in your ESP-IDF projects with the full flexibility of the ESP-IDF.

This method is recommended for advanced users. To use this method, you will need to have the ESP-IDF toolchain installed.

For a simplified method, see `Installing using Boards Manager <https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html#installing-using-boards-manager>`_.

If you plan to use these modified settings multiple times, for different projects and targets, you can recompile the Arduino core with the new settings using the Arduino Static Library Builder.
For more information, see the `Lib Builder documentation <lib_builder.html>`_.

.. note:: Latest Arduino Core ESP32 version (3.0.X) is now compatible with `ESP-IDF v5.1 <https://github.com/espressif/esp-idf/tree/release/v5.1>`_. Please consider this compatibility when using Arduino as a component in ESP-IDF.

For easiest use of Arduino framework as a ESP-IDF component, you can use the `IDF Component Manager <https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32/api-guides/tools/idf-component-manager.html>`_ to add the Arduino component to your project.
This will automatically clone the repository and its submodules. You can find the Arduino component in the `ESP Registry <https://components.espressif.com/components/espressif/arduino-esp32>`_ together with dependencies list and examples.

Installation
------------

#. Download and install `ESP-IDF <https://github.com/espressif/esp-idf>`_.

   * For more information see `Get Started <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation-step-by-step>`_.

Installing using IDF Component Manager
**************************************

To add the Arduino component to your project using the IDF Component Manager, run the following command in your project directory:

.. code-block:: bash

    idf.py add-dependency "espressif/arduino-esp32^3.0.2"

Or you can start a new project from a template with the Arduino component:

.. code-block:: bash

    idf.py create-project-from-example "espressif/arduino-esp32^3.0.2:hello_world"

Manual installation of Arduino framework
****************************************

#. Create a blank ESP-IDF project (use sample_project from /examples/get-started) or choose one of the examples.
#. In the project folder, create a new folder called ``components`` and clone this repository inside the newly created folder.

.. code-block:: bash

    mkdir -p components && \
    cd components && \
    git clone https://github.com/espressif/arduino-esp32.git arduino && \
    cd arduino && \
    git submodule update --init --recursive && \
    cd ../.. && \
    idf.py menuconfig

.. note:: If you use Arduino with ESP-IDF often, you can place the arduino folder into global components folder.

If you're targeting the ESP32-S2 or ESP32-S3 and you want to use USBHID classes such as ``USBHID``, ``USBHIDConsumerControl``, ``USBHIDGamepad``, ``USBHIDKeyboard``, ``USBHIDMouse``, ``USBHIDSystemControl``, or ``USBHIDVendor``:

1. Clone these nested repos somewhere:

.. code-block:: bash

    git clone https://github.com/espressif/esp32-arduino-lib-builder.git esp32-arduino-lib-builder && \
    git clone https://github.com/hathach/tinyusb.git esp32-arduino-lib-builder/components/arduino_tinyusb/tinyusb

2. In the project folder, edit ``CMakeLists.txt`` and add the following before the ``project()`` line:

.. code-block:: bash

    set(EXTRA_COMPONENT_DIRS <path to esp32-arduino-lib-builder/components/arduino_tinyusb>)

Configuration
-------------

Depending on one of the two following options, in the menuconfig set the appropriate settings.

Go to the section ``Arduino Configuration --->``

1. For usage of ``app_main()`` function - Turn off ``Autostart Arduino setup and loop on boot``
2. For usage of ``setup()`` and ``loop()`` functions - Turn on ``Autostart Arduino setup and loop on boot``

Experienced users can explore other options in the Arduino section.

After the setup you can save and exit:

- Save [S]
- Confirm default filename [Enter]
- Close confirmation window [Enter] or [Space] or [Esc]
- Quit [Q]

As the Arduino libraries use C++ features, you will need to swap some file extensions from ``.c`` to ``.cpp``:

- In main folder rename file `main.c` to `main.cpp`.
- In main folder open file `CMakeLists.txt` and change `main.c` to `main.cpp` as described below.

Option 1. Using Arduino setup() and loop()
******************************************

Your main.cpp should be formatted like any other sketch. Don't forget to include ``Arduino.h``.

.. code-block:: cpp

    //file: main.cpp
    #include "Arduino.h"

    void setup(){
      Serial.begin(115200);
      while(!Serial){
        ; // wait for serial port to connect
      }
    }

    void loop(){
        Serial.println("loop");
        delay(1000);
    }

Option 2. Using ESP-IDF appmain()
*********************************

In main.cpp you need to implement ``app_main()`` and call ``initArduino();`` in it.

Keep in mind that setup() and loop() will not be called in this case.
Furthermore the ``app_main()`` is single execution as a normal function so if you need an infinite loop as in Arduino place it there.

.. code-block:: cpp

    //file: main.cpp
    #include "Arduino.h"

    extern "C" void app_main()
    {
      initArduino();

      // Arduino-like setup()
      Serial.begin(115200);
      while(!Serial){
        ; // wait for serial port to connect
      }

      // Arduino-like loop()
      while(true){
        Serial.println("loop");
      }

      // WARNING: if program reaches end of function app_main() the MCU will restart.
    }

Build, flash and monitor
************************

- For both options use command ``idf.py -p <your-board-serial-port> flash monitor``

- The project will build, upload and open the serial monitor to your board

  - Some boards require button combo press on the board: press-and-hold Boot button + press-and-release RST button, release Boot button

  - After a successful flash, you may need to press the RST button again

  - To terminate the serial monitor press ``Ctrl`` + ``]``

Logging To Serial
-----------------

If you are writing code that does not require Arduino to compile and you want your `ESP_LOGx` macros to work in Arduino IDE, you can enable the compatibility by adding the following lines:

.. code-block:: c

    #ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
    #endif

FreeRTOS Tick Rate (Hz)
-----------------------

The Arduino component requires the FreeRTOS tick rate `CONFIG_FREERTOS_HZ` set to 1000 Hz in `make menuconfig` -> `Component config` -> `FreeRTOS` -> `Tick rate`.

Compilation Errors
------------------

As commits are made to ESP-IDF and submodules, the codebases can develop incompatibilities that cause compilation errors.
If you have problems compiling, follow the instructions in `Issue #1142 <https://github.com/espressif/arduino-esp32/issues/1142>`_
to roll ESP-IDF back to a different version.

Adding arduino library
----------------------

There are few approaches:

1. Add global library to ``components/arduino-esp32/libraries/new_library``
2. Add local project library to ``examples/your_project/main/libraries/new_library``

1 Adding global library
***********************

Download the library:

.. code-block:: bash

    cd ~/esp/esp-idf/components/arduino/
    git clone --recursive git@github.com:Author/new_library.git libraries/new_library


Edit file ``components/arduino-esp32/CMakeLists.txt``

Get the source file list with shell command:

.. code-block:: bash

    find libraries/new_library/src/ -name '*.c' -o -name '*.cpp'
      libraries/new_library/src/new_library.cpp
      libraries/new_library/src/new_library_extra_file.c

Locate block which starts with ``set(LIBRARY_SRCS`` and copy the list there. Now it should look something like this:

.. code-block:: bash

    set(LIBRARY_SRCS
      libraries/ArduinoOTA/src/ArduinoOTA.cpp
      libraries/AsyncUDP/src/AsyncUDP.cpp
      libraries/new_library/src/new_library.cpp
      libraries/new_library/src/new_library_extra_file.c


After this add the library path to block which starts with ``set(includedirs``. It should look like this:

.. code-block:: bash

    set(includedirs
      variants/${CONFIG_ARDUINO_VARIANT}/
      cores/esp32/
      libraries/ArduinoOTA/src
      libraries/AsyncUDP/src
      libraries/new_library/src


2 Adding local library
**********************

Download the library:

.. code-block:: bash

    cd ~/esp/esp-idf/examples/your_project
    mkdir components
    git clone --recursive git@github.com:Author/new_library.git components/new_library

Create new CMakeists.txt in the library folder: ``components/new_library/CMakeLists.txt``

.. code-block:: bash

    idf_component_register(SRCS "new_library.cpp" "another_source.c"
                          INCLUDE_DIRS "."
                          REQUIRES arduino-esp32
                          )

You can read more about CMakeLists in the IDF documentation regarding the `Build System <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html>`_

Tip
---

If you want to use arduino-esp32 both as an ESP-IDF component and with Arduino IDE you can simply create a symlink:

.. code-block:: bash

    ln -s ~/Arduino/hardware/espressif/esp32  ~/esp/esp-idf/components/arduino-esp32

This will allow you to install new libraries as usual with Arduino IDE. To use them with IDF component, use ``add_lib.sh -e ~/Arduino/libraries/New_lib``
