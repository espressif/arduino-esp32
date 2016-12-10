# Arduino core for ESP32 WiFi chip

- [Development Status](#development-status)
- Installing options:
  + [Using Arduino IDE](#using-arduino-ide)
  + [Using PlatformIO](#using-platformio)
  + [Using as ESP-IDF component](#using-as-esp-idf-component)
- [ESP32Dev Board PINMAP](#esp32dev-board-pinmap)  

## Development Status
Not everything is working yet, you can not get it through package manager, but you can give it a go and help us find bugs in the things that are implemented :)

The framework can also be downloaded as component in an IDF project and be used like that.

Things that work:

- pinMode
- digitalRead/digitalWrite
- attachInterrupt/detachInterrupt
- analogRead/touchRead/touchAttachInterrupt
- ledcWrite/sdWrite/dacWrite
- Serial (global Serial is attached to pins 1 and 3 by default, there are another 2 serials that you can attach to any pin)
- SPI (global SPI is attached to VSPI pins by default and HSPI can be attached to any pins)
- Wire (global Wire is attached to pins 21 and 22 by default and there is another I2C bus that you can attach to any pins)
- WiFi (about 99% the same as ESP8266)

WiFiClient, WiFiServer and WiFiUdp are not quite ready yet because there are still some small hiccups in LwIP to be overcome.
You can try WiFiClient but you need to disconnect the client yourself to be sure that connection is closed.

## Using Arduino IDE

###[Instructions for Windows](doc/windows.md)

### Instructions for Mac
- Install latest Arduino IDE from [arduino.cc](https://www.arduino.cc/en/Main/Software)
- Open Terminal and execute the following command (copy->paste and hit enter):

  ```bash
  curl -o get-pip.py https://bootstrap.pypa.io/get-pip.py && \
  sudo python get-pip.py && \
  sudo pip install pyserial && \
  mkdir -p ~/Documents/Arduino/hardware/espressif && \
  cd ~/Documents/Arduino/hardware/espressif && \
  git clone https://github.com/espressif/arduino-esp32.git esp32 && \
  cd esp32/tools/ && \
  python get.py
  ```
- Restart Arduino IDE

### Instructions for Debian/Ubuntu Linux
- Install latest Arduino IDE from [arduino.cc](https://www.arduino.cc/en/Main/Software)
- Open Terminal and execute the following command (copy->paste and hit enter):

  ```bash
  sudo usermod -a -G dialout $USER && \
  sudo apt-get install git && \
  wget https://bootstrap.pypa.io/get-pip.py && \
  sudo python get-pip.py && \
  sudo pip install pyserial && \
  mkdir -p ~/Arduino/hardware/espressif && \
  cd ~/Arduino/hardware/espressif && \
  git clone https://github.com/espressif/arduino-esp32.git esp32 && \
  cd esp32/tools/ && \
  python get.py
  ```
- Restart Arduino IDE

## Using PlatformIO

[PlatformIO](http://platformio.org) is an open source ecosystem for IoT
development with cross platform build system, library manager and full support
for Espressif ESP32 development. It works on the popular host OS: Mac OS X, Windows,
Linux 32/64, Linux ARM (like Raspberry Pi, BeagleBone, CubieBoard).

- [What is PlatformIO?](http://docs.platformio.org/page/what-is-platformio.html)
- [PlatformIO IDE](http://platformio.org/platformio-ide)
- Quick Start with [PlatformIO IDE](http://docs.platformio.org/page/ide/atom.html#quick-start) or [PlatformIO Core](http://docs.platformio.org/page/core.html)
- [Integration with Cloud and Standalone IDEs](http://docs.platformio.org/page/ide.html) -
  Cloud9, Codeanywehre, Eclipse Che (Codenvy), Atom, CLion, Eclipse, Emacs, NetBeans, Qt Creator, Sublime Text, VIM and Visual Studio
- [Project Examples](https://github.com/platformio/platform-espressif32/tree/develop/examples)

## Using as ESP-IDF component
- Download and install [esp-idf](https://github.com/espressif/esp-idf)
- Create blank idf project (from one of the examples)
- in the project folder, create a folder called components and clone this repository inside
    
    ```bash
    mkdir -p components && \
    cd components && \
    git clone https://github.com/espressif/arduino-esp32.git arduino && \
    cd.. && \
    make menuconfig
  ```
- ```make menuconfig``` has some Arduino options
    - "Autostart Arduino setup and loop on boot"
        - If you enable this options, your main.cpp should be formated like any other sketch
          
          ```arduino
          //file: main.cpp
          #include "Arduino.h"
          
          void setup(){
            Serial.begin(115200);
          }
          
          void loop(){
            Serial.println("loop");
            delay(1000);
          }
          ```
        - Else you need to implement ```app_main()``` and call ```initArduino();``` in it.
        
          Keep in mind that setup() and loop() will not be called in this case
          
          ```arduino
          //file: main.cpp
          #include "Arduino.h"
          extern "C" void initArduino();
          
          extern "C" void app_main()
          {
              initArduino();
              pinMode(4, OUPUT);
              digitalWrite(4, HIGH);
              //do your own thing
          }
          ```
    - "Disable mutex locks for HAL"
        - If enabled, there will be no protection on the drivers from concurently accessing them from another thread/interrupt/core
    - "Autoconnect WiFi on boot"
        - If enabled, WiFi will start with the last known configuration
        - Else it will wait for WiFi.begin
- ```make flash monitor``` will build, upload and open serial monitor to your board

## ESP32Dev Board PINMAP

![Pin Functions](doc/esp32_pinmap.png)
