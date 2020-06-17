# LITTLEFS
## LittleFS library for arduino-esp32

- A LittleFS wrapper for Arduino ESP32 of [Mbed LittleFS](https://github.com/ARMmbed/littlefs)
- Based on [ESP-IDF port of joltwallet/esp_littlefs](https://github.com/joltwallet/esp_littlefs) , thank you Brian!
- Independent library, no need ESP-IDF SDK and arduino + littlefs component recompiling / re-integration, but it could be implemented this way, too.
- Functionality is the same and SPIFFS partition scheme and data folder meaning are kept
- You can use either LITTLEFS or SPIFFS but not both simultaneously on given Arduino project

### Installation

- <b>So far it is tested only with commit b92c58d of arduino-esp32 core (IDF 3.3). The release 1.0.4 is on an older IDF!</b>
- Copy <b>LITTLEFS</b> folder to Arduino IDE embedded libraries place
- For Win, the default place of arduino-esp32 core libraries is somewhere like: 
```C:\Users\<username>\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.4\libraries ```
- Alternatively, you can put it to your usual libraries place

### Usage

- In your existing code, replace SPIFFS like this 
``` 
#define USE_LittleFS

#include <FS.h>
#ifdef USE_LittleFS
  #define SPIFFS LITTLEFS
  #include <LITTLEFS.h> 
#else
  #include <SPIFFS.h>
#endif 
 ```
### Differences with SPIFFS (and in this implementation)

- LittleFS has folders, you my need to tweak your code to iterate files in folders
- Root: /someting  = something, so attention to /
- Lower level littlefs library cannot mount on NULL as partition_label name, while SPIFFS can
- Lower level littlefs library does not need maxOpenFiles parameter
- Speed (LITTLEFS_Test.ino) 1048576 bytes written in 16238 ms / 1048576 bytes read in 918 ms
- Speed (SPIFFS_Test.ino)   1048576 bytes written in 65971 ms / 1048576 bytes read in 680 ms


### Arduino ESP32 LittleFS filesystem upload tool 

- Download the tool archive from [here](https://github.com/lorol/arduino-esp32littlefs-plugin/raw/master/src/bin/esp32littlefs.jar)
- In your Arduino sketchbook directory, create tools directory if it doesn't exist yet.
- Unpack the tool into tools directory (the path will look like ```<home_dir>/Arduino/tools/ESP32LittleFS/tool/esp32littlefs.jar```).
- You need the [mklittlefs tool](https://github.com/earlephilhower/mklittlefs)  Download the [release](https://github.com/earlephilhower/mklittlefs/releases) and copy it to 
packages\esp32\tools\mkspiffs\<mklittlefs rev. x.x.x>\ or on checkout (dev) environment to: packages\esp32\hardware\esp32\<release>\tools\mklittlefs\
- Restart Arduino IDE. 

## Credits and license

- This work is based on [Mbed LittleFS](https://github.com/ARMmbed/littlefs) , [ESP-IDF port of joltwallet/esp_littlefs](https://github.com/joltwallet/esp_littlefs) , [Espressif Arduino core for the ESP32, the ESP-IDF - SPIFFS Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/SPIFFS)
- Licensed under GPL v2 ([text](LICENSE))

