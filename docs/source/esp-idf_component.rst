##############################
Arduino as a ESP-IDF component
##############################

ESP32 Arduino lib-builder
-------------------------

For a simplified method, see `Installing using Boards Manager <https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html#installing-using-boards-manager>`_.

To build your own Arduino core see `Arduino lib builder <https://github.com/espressif/esp32-arduino-lib-builder>`_


Installation
------------

.. note:: Latest Arduino Core ESP32 version is now compatible with `ESP-IDF v4.4 <https://github.com/espressif/esp-idf/tree/release/v4.4>`_. Please consider this compability when using Arduino as component in ESP-IDF.

- Download and install `ESP-IDF <https://github.com/espressif/esp-idf>`_.

  - For more information see `Installation step by step <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation-step-by-step>`_.
- Create blank ESP-IDF project (use sample_project from /examples/get-started) or choose one of the examples.
- In the project folder, create a new folder called `components` and clone this repository inside the new created folder.

.. code-block:: bash
    
    mkdir -p components && \
    cd components && \
    git clone https://github.com/espressif/arduino-esp32.git arduino && \
    cd arduino && \
    git submodule update --init --recursive && \
    cd ../.. && \
    idf.py menuconfig

Configuration
-------------

Depending on one the two following options, in the menuconfig set the appropriate settings.
Go to section ``Arduino Configuration --->``

1. For usage of ``app_main()`` function - Turn off ``Autostart Arduino setup and loop on boot``
2. For usage of ``setup()`` and ``loop()`` functions - Turn on ``Autostart Arduino setup and loop on boot``

Experienced users can explore other options in the Arduino section.

After the setup you can save and exit:

- Save [S]

- Confirm default filename [Enter]

- Close confirmation window [Enter] or [Space] or [Esc]

- Quit [Q]

Option 1. Using Arduino setup() and loop()
******************************************

- In main folder rename file `main.c` to `main.cpp`.

- In main folder open file `CMakeList.txt` and change `main.c` to `main.cpp` as described below.

- Your main.cpp should be formated like any other sketch.

.. code-block:: c

    //file: main.cpp
    #include "Arduino.h"

    void setup(){
        Serial.begin(115200);
    }

    void loop(){
        Serial.println("loop");
        delay(1000);
    }

Option 2. Using ESP-IDF appmain()
*********************************

In main.c or main cpp you need to implement ``app_main()`` and call ``initArduino();`` in it.

Keep in mind that setup() and loop() will not be called in this case.
Furthermore the ``app_main()`` is single execution as normal function so if you need infinite loop as in Arduino place it there.

.. code-block:: cpp

    //file: main.c or main.cpp
    #include "Arduino.h"

    extern "C" void app_main()
    {
      initArduino();

      // Arduino-like setup()
      Serial.begin(115200);

      // Arduino-like loop()
      while(!Serial){
        Serial.println("loop");
        delay(1000);
      }

      // WARNING: if program reaches end of function app_main() the MCU will restart.
    }

Build, flash and monitor
************************

- For both options use command ``idf.py -p <your-board-serial-port> flash monitor``

  - The port is usually ``/dev/ttyUSB0`` search the active port with ``ls /dev/ttyUSB*``

- The project will build, upload and open serial monitor to your board

  - Some boards require button combo press on the board: press-and-hold Boot button + press-and-release RST button, release Boot button

  - After successful flash you may need to press RST button again

  - To terminate the serial monitor press [Ctrl] + [ ] ]

Logging To Serial
-----------------

If you are writing code that does not require Arduino to compile and you want your `ESP_LOGx` macros to work in Arduino IDE, you can enable the compatibility by adding the following lines after:

.. code-block:: c

    #ifdef ARDUINO_ARCH_ESP32
    #include "esp32-hal-log.h"
    #endif

FreeRTOS Tick Rate (Hz)
-----------------------

You might notice that Arduino-esp32's `delay()` function will only work in multiples of 10ms. That is because, by default, esp-idf handles task events 100 times per second.
To fix that behavior, you need to set FreeRTOS tick rate to 1000Hz in `make menuconfig` -> `Component config` -> `FreeRTOS` -> `Tick rate`.

Compilation Errors
------------------

As commits are made to esp-idf and submodules, the codebases can develop incompatibilities which cause compilation errors.  If you have problems compiling, follow the instructions in `Issue #1142 <https://github.com/espressif/arduino-esp32/issues/1142>`_ to roll esp-idf back to a different version.
