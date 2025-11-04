# Arduino ESP-Matter over Thread example using ESP32-C6
This is an Arduino as IDF Project to build an ESP-Matter over Thread RGB Light using ESP32-C6 and ESP-Matter Arduino API \
This example shall work with Arduino 3.0.5+ and also IDF 5.1.4+\
It is necessary to make sure that the IDF version matches with the one used to release the Arduino Core version.\
This can be done looking into release information in https://github.com/espressif/arduino-esp32/releases \

Any example from [ESP32 Matter Library examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/Matter/examples)
can be used to build the application.\
Feel free to create your own Arduino Matter sketch!\ 
Do not forget to rename the `sketch_file_name.ino` to `sketch_file_name.cpp` in `main` folder.

The `main/idf_component.yml` file holds the ESP-Matter component version and Arduino Core version.\
Edit this file to set the target versions, if necessary.

# General Instructions:

1- Install the required IDF version into your computer. It can be done following the guide in
https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html

For Windows: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html \
For Linux or MacOS: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/linux-macos-setup.html

2- Test IDF with `idf.py --version` to check if it is installed and configured correctly.

3- To create a ESP-IDF project from this example with the latest release of Arduino-esp32, you can simply run command:
`idf.py create-project-from-example "espressif/arduino-esp32:Arduino_ESP-Matter-over-Thread_ESP32_C6"`
ESP-IDF will download all dependencies needed from the component registry and setup the project for you.

4- Open an IDF terminal and execute `idf.py set-target esp32c6`

5- Execute `idf.py -p <your COM or /dev/tty port connected to the ESP32-C6> flash monitor`

6- It will build, upload and show the UART0 output in the screen.

7- Try to add the Matter RGB light to your Matter environment.
