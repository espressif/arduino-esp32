/*
 * pin 1 - D2                |  Micro SD card     |
 * pin 2 - D3                |                   /
 * pin 3 - CMD               |                  |__
 * pin 4 - VDD (3.3V)        |                    |
 * pin 5 - CLK               | 8 7 6 5 4 3 2 1   /
 * pin 6 - VSS (GND)         | ▄ ▄ ▄ ▄ ▄ ▄ ▄ ▄  /
 * pin 7 - D0                | ▀ ▀ █ ▀ █ ▀ ▀ ▀ |
 * pin 8 - D1                |_________________|
 *                             ║ ║ ║ ║ ║ ║ ║ ║
 *                     ╔═══════╝ ║ ║ ║ ║ ║ ║ ╚═════════╗
 *                     ║         ║ ║ ║ ║ ║ ╚══════╗    ║
 *                     ║   ╔═════╝ ║ ║ ║ ╚═════╗  ║    ║
 * Connections for     ║   ║   ╔═══╩═║═║═══╗   ║  ║    ║
 * full-sized          ║   ║   ║   ╔═╝ ║   ║   ║  ║    ║
 * SD card             ║   ║   ║   ║   ║   ║   ║  ║    ║
 * ESP32-S3 DevKit  | 21  47  GND  39 3V3 GND  40 41  42  |
 * ESP32-S3-USB-OTG | 38  37  GND  36 3V3 GND  35 34  33  |
 * ESP32            |  4   2  GND  14 3V3 GND  15 13  12  |
 * Pin name         | D1  D0  VSS CLK VDD VSS CMD D3  D2  |
 * SD pin number    |  8   7   6   5   4   3   2   1   9 /
 *                  |                                  █/
 *                  |__▍___▊___█___█___█___█___█___█___/
 * WARNING: ALL data pins must be pulled up to 3.3V with external 10k Ohm resistor!
 * Note to ESP32 pin 2 (D0) : add 1K pull up after flashing
 *
 * For more info see file README.md in this library or on URL:
 * https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC
 */

#include "FS.h"
#include "SD_MMC.h"

// Default pins for ESP-S3
// Warning: ESP32-S3-WROOM-2 is using most of the default SD_MMC GPIOs (33-37) to interface with on-board OPI flash.
//   If the SD_MMC is initialized with default pins it will result in rebooting loop - please
//   reassign the pins elsewhere using the mentioned command `setPins`.
// Note: ESP32-S3-WROOM-1 does not have GPIO 33 and 34 broken out.
// Note: The board SP32-S3-USB-OTG has predefined default pins and the following definitions with the setPins() call will not be compiled.
// Note: Pins in this definition block are ordered from top down in order in which they are on the full-sized SD card
//       from left to right when facing the pins down (when connected to a breadboard)

#if defined(SOC_SDMMC_USE_GPIO_MATRIX) && not defined(BOARD_HAS_SDMMC)
    int d1  = 21; // SD pin 8 - add 10k Pullup to 3.3V if using 4-bit mode (use_1_bit_mode = false)
    int d0  = 47; // SD pin 7 - add 10k Pullup
    //     GND pin - SD pin 6
    int clk = 39; // SD pin 5 - add 10k Pullup
    //    3.3V pin - SD pin 4
    //     GND pin - SD pin 3
    int cmd = 40; // SD pin 2 - add 10k Pullup
    int d3  = 41; // SD pin 1 - add 10k Pullup to card's pin even when using 1-bit mode
    int d2  = 42; // SD pin 9 - add 10k Pullup to 3.3V if using 4-bit mode (use_1_bit_mode = false)
#endif

bool use_1_bit_mode = false; // Change the value to `true` to use 1-bit mode instead of the 4-bit mode

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %lu ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
    file.close();
}

void setup(){
    Serial.begin(115200);
    while(!Serial) { delay(10); }
    Serial.println("SDMMC_Test.ino starting!");

    // If you are using any other ESP32-S3 board than ESP32-S3-USB-OTG which has preset default pins, you will
    //    need to specify the pins with the following example of SD_MMC.setPins()
    // If you want to use only 1-bit mode, you can use the line with only one data pin (d0) begin changed.
    // Please note that ESP32 does not allow pin change and will fail unless you enter the same pin config as is the hardwired.
#if defined(SOC_SDMMC_USE_GPIO_MATRIX) && not defined(BOARD_HAS_SDMMC)
    //if(! SD_MMC.setPins(clk, cmd, d0)){ // 1-bit line version
    if(! SD_MMC.setPins(clk, cmd, d0, d1, d2, d3)){ // 4-bit line version
        Serial.println("Pin change failed!");
        return;
    }
#endif

/*
// This is waiting for implementation in peripheral manager (periman)
    if(use_1_bit_mode){
        Serial.printf("Begin in 1-bit mode; pins: CLK=%d, CMD=%d, D0=%d\n", SD_MMC.getClkPin(), SD_MMC.getCmdPin(), SD_MMC.getD0Pin());
    }else{
        Serial.printf("Begin in 4-bit mode; pins: CLK=%d, CMD=%d, D0=%d, D1=%d, D2=%d, D3=%d\n", SD_MMC.getClkPin(), SD_MMC.getCmdPin(), SD_MMC.getD0Pin(), SD_MMC.getD1Pin(), SD_MMC.getD2Pin(), SD_MMC.getD3Pin());
    }
*/

    if(!SD_MMC.begin("/sdcard", use_1_bit_mode)){
        Serial.println("Card Mount Failed.");
        Serial.println("Increase log level to see more info: Tools > Core Debug Level > Verbose");
        Serial.println("Make sure that all data pins have 10 kOhm pull-up resistor");
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
        Serial.println("Make sure that when using generic ESP32-S3 board the pins are setup using SD_MMC.setPins()");
#endif
        return;
    }
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        return;
    }

    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

    listDir(SD_MMC, "/", 0);
    createDir(SD_MMC, "/mydir");
    listDir(SD_MMC, "/", 0);
    removeDir(SD_MMC, "/mydir");
    listDir(SD_MMC, "/", 2);
    writeFile(SD_MMC, "/hello.txt", "Hello ");
    appendFile(SD_MMC, "/hello.txt", "World!\n");
    readFile(SD_MMC, "/hello.txt");
    deleteFile(SD_MMC, "/foo.txt");
    renameFile(SD_MMC, "/hello.txt", "/foo.txt");
    readFile(SD_MMC, "/foo.txt");
    testFileIO(SD_MMC, "/test.txt");
    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
}

void loop(){

}
