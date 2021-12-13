##############################
Arduino as a ESP-IDF component
##############################

ESP32 Arduino lib-builder
-------------------------

For a simplified method, see `lib-builder <https://github.com/espressif/esp32-arduino-lib-builder>`_.

Installation
------------

.. note:: Latest Arduino Core ESP32 version is now compatible with `ESP-IDF v4.4 <https://github.com/espressif/esp-idf/tree/release/v4.4>`_. Please consider this compability when using Arduino as component in ESP-IDF.

- Download and install `ESP-IDF <https://github.com/espressif/esp-idf>`_.
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

Option 1. Using Arduino setup() and loop()
******************************************

- The `idf.py menuconfig` has some Arduino options.
    - Turn on `Autostart Arduino setup and loop on boot`.
    - In main folder rename file `main.c` to `main.cpp`.
    - In main folder open file `CMakeList.txt` and change `main.c` to `main.cpp` as described below.

.. code-block:: bash

    idf_component_register(SRCS "main.cpp" INCLUDE_DIRS ".")

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

- You need to implement ``app_main()`` and call ``initArduino();`` in it.

Keep in mind that setup() and loop() will not be called in this case.
If you plan to base your code on examples provided in `examples <https://github.com/espressif/esp-idf/tree/master/examples>`_, please make sure to move the app_main() function in main.cpp from the files in the example.

.. code-block:: cpp

    //file: main.c or main.cpp
    #include "Arduino.h"

    extern "C" void app_main()
    {
        initArduino();
        pinMode(4, OUTPUT);
        digitalWrite(4, HIGH);
        //do your own thing
    }

- "Disable mutex locks for HAL"
- If enabled, there will be no protection on the drivers from concurently accessing them from another thread/interrupt/core
    - "Autoconnect WiFi on boot"
- If enabled, WiFi will start with the last known configuration
- Otherwise it will wait for WiFi.begin

Build, flash and monitor
************************

- For both options use command ``idf.py -p <your-board-serial-port> flash monitor``
- It will build, upload and open serial monitor to your board.

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
