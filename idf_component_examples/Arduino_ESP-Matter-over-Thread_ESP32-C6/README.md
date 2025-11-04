# Arduino ESP-Matter over Thread example using ESP32-C6
This is an Arduino as IDF Project to build an ESP-Matter over Thread RGB Light using ESP32-C6 and ESP-Matter Arduino API \
The example requires IDF 5.5.0 and ESP32 Arduino Core 3.0.0

# Instructions:

1- Install IDF 5.5.0 into your computer. It can be done following the guide in
https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html

For Windows: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html \
For Linux or MacOS: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/linux-macos-setup.html

2- Test IDF with `idf.py --version` to check if it is installed and configured correctly.
<img width="539" height="215" alt="image" src="https://github.com/user-attachments/assets/ed95e767-9451-4e03-ab85-f83e08a2f936" />

3- Clone the repository with the Arduino as IDF Component project.
`git clone https://github.com/SuGlider/Arduino_ESP-Matter-over-Thread_ESP32-C6`

4- Open an IDF terminal and execute `idf.py set-target esp32c6`

5- Execute `idf.py -p <your COM or /dev/tty port connected to the ESP32-C6> flash monitor`

6- It will build, upload and show the UART0 output in the screen.

7- Try to add the Matter RGB light to your Matter environment.
