###############
Library Builder
###############

How to Use Library Builder
--------------------------

Espressif has provided a `tool <https://github.com/espressif/esp32-arduino-lib-builder>`_ to simplify building your own compiled libraries for use in Arduino IDE (or your favorite IDE).
To generate custom libraries, follow these steps:


- Step 1 - Clone the ESP32 Arduino lib builder::

    git clone https://github.com/espressif/esp32-arduino-lib-builder

- Step 2 - Go to the ``esp32-arduino-lib-builder`` folder::

    cd esp32-arduino-lib-builder

- Step 3 - Run the ``update-components`` script::

    ./tools/update-components.sh`

- Step 4 - Run ``install-esp-idf`` installation script (if you already have an ``$IDF_PATH`` defined, it will use your local copy of the repository)::

    ./tools/install-esp-idf.sh

- Step 5 - Copy the configuration (recommended) or directly edit sdkconfig using ``idf.py menuconfig``::

    cp sdkconfig.esp32s2 sdkconfig

- Step 6 - Build::

    idf.py build

The script automates the process of building `Arduino as an ESP-IDF component <https://github.com/espressif/arduino-esp32/blob/master/docs/esp-idf_component.md>`_.
Once it is complete, you can cherry pick the needed libraries from ``out/tools/sdk/lib``, or run ``tools/copy-to-arduino.sh`` to copy the entire built system.
``tools/config.sh`` contains a number of variables that control the process, particularly the ``$IDF_BRANCH`` variable.  You can adjust this to try building against newer versions, but there are absolutely no guarantees that any components will work or even successfully compile against a newer IDF.
