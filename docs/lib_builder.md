## Using esp32-arduino-lib-builder to compile custom libraries

Espressif has provided a [tool](https://github.com/espressif/esp32-arduino-lib-builder) to simplify building your own compiled libraries for use in Arduino IDE (or your favorite IDE).
To use it to generate custom libraries, follow these steps:
1. `git clone https://github.com/espressif/esp32-arduino-lib-builder`
2. `cd esp32-arduino-lib-builder`
3. `./tools/update-components.sh`
4. `./tools/install-esp-idf.sh` (if you already have an $IDF_PATH defined, it will use your local copy of the repository)
5. Edit sdkconfig as needed, or run make menuconfig.
6. `./build.sh`

The script automates the process or building [arduino as an ESP-IDF component](https://github.com/espressif/arduino-esp32/blob/master/docs/esp-idf_component.md).
Once it is complete, you can cherry pick the needed libraries from `out/tools/sdk/lib`, or run `tools/copy-to-arduino.sh` to copy the entire built system.
`tools/config.sh` contains a number of variables that control the process, particularly the $IDF_BRANCH variable.  You can adjust this to try building against newer versions, but there are absolutely no guarantees than any components will work or even succesfully compile against a newer IDF.
