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
 * ESP32-P4 Func EV | 40  39  GND  43 3V3 GND  44 43  42  | SLOT 0 (IO_MUX)
 * ESP32-S3 DevKit  | 21  47  GND  39 3V3 GND  40 41  42  |
 * ESP32-S3-USB-OTG | 38  37  GND  36 3V3 GND  35 34  33  |
 * ESP32            |  4   2  GND  14 3V3 GND  15 13  12  |
 * Pin name         | D1  D0  VSS CLK VDD VSS CMD D3  D2  |
 * SD pin number    |  8   7   6   5   4   3   2   1   9 /
 *                  |                                  █/
 *                  |__▍___▊___█___█___█___█___█___█___/
 * WARNING: ALL data pins must be pulled up to 3.3V with external 10k Ohm resistor!
 * Note to ESP32 pin 2 (D0): Add 1K Ohm pull-up resistor to 3.3V after flashing
 *
 * For more info see file README.md in this library or on URL:
 * https://github.com/espressif/arduino-esp32/tree/master/libraries/SD_MMC
 */

#include "FS.h"
#include "SD_MMC.h"
#include "SPI.h"
#include <time.h>
#include <WiFi.h>

const char *ssid = "your-ssid";
const char *password = "your-password";

long timezone = 1;
byte daysavetime = 1;

// Default pins for ESP-S3
// Warning: ESP32-S3-WROOM-2 is using most of the default SD_MMC GPIOs (33-37) to interface with on-board OPI flash.
//   If the SD_MMC is initialized with default pins it will result in rebooting loop - please
//   reassign the pins elsewhere using the mentioned command `setPins`.
// Note: ESP32-S3-WROOM-1 does not have GPIO 33 and 34 broken out.
// Note: The board ESP32-S3-USB-OTG has predefined default pins and the following definitions with the setPins() call will not be compiled.
// Note: Pins in this definition block are ordered from top down in order in which they are on the full-sized SD card
//   from left to right when facing the pins down (when connected to a breadboard)

#if defined(SOC_SDMMC_USE_GPIO_MATRIX) && not defined(BOARD_HAS_SDMMC)
int d1 = 21;  // SD pin 8 - Add a 10k Ohm pull-up resistor to 3.3V if using 4-bit mode (use_1_bit_mode = false)
int d0 = 47;  // SD pin 7 - Add a 10k Ohm pull-up resistor to 3.3V
//     GND pin - SD pin 6
int clk = 39;  // SD pin 5 - Add a 10k Ohm pull-up resistor to 3.3V
//    3.3V pin - SD pin 4
//     GND pin - SD pin 3
int cmd = 40;  // SD pin 2 - Add a 10k Ohm pull-up resistor to 3.3V
int d3 = 41;   // SD pin 1 - Add a 10k Ohm pull-up resistor to 3.3V to card's pin even when using 1-bit mode
int d2 = 42;   // SD pin 9 - Add a 10k Ohm pull-up resistor to 3.3V if using 4-bit mode (use_1_bit_mode = false)
#endif

bool use_1_bit_mode = false;  // Change the value to `true` to use 1-bit mode instead of the 4-bit mode

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.print(file.name());
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      Serial.printf(
        "  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour,
        tmstruct->tm_min, tmstruct->tm_sec
      );
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.print(file.size());
      time_t t = file.getLastWrite();
      struct tm *tmstruct = localtime(&t);
      Serial.printf(
        "  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour,
        tmstruct->tm_min, tmstruct->tm_sec
      );
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void setup() {
  Serial.begin(115200);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Contacting Time Server");
  configTime(3600 * timezone, daysavetime * 3600, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org");
  struct tm tmstruct;
  delay(2000);
  tmstruct.tm_year = 0;
  getLocalTime(&tmstruct, 5000);
  Serial.printf(
    "\nNow is : %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct.tm_year) + 1900, (tmstruct.tm_mon) + 1, tmstruct.tm_mday, tmstruct.tm_hour, tmstruct.tm_min,
    tmstruct.tm_sec
  );
  Serial.println("");

  // If you are using any other ESP32-S3 board than ESP32-S3-USB-OTG which has preset default pins, you will
  // need to specify the pins with the following example of SD_MMC.setPins()
  // If you want to use only 1-bit mode, you can use the line with only one data pin (d0) begin changed.
  // Please note that ESP32 does not allow pin changes and will fail unless you enter the same pin config as is the hardwired.
#if defined(SOC_SDMMC_USE_GPIO_MATRIX) && not defined(BOARD_HAS_SDMMC)
  if (use_1_bit_mode) {
    if (!SD_MMC.setPins(clk, cmd, d0)) {  // 1-bit line version
      Serial.println("Pin change failed!");
      return;
    }
  } else {
    if (!SD_MMC.setPins(clk, cmd, d0, d1, d2, d3)) {  // 4-bit line version
      Serial.println("Pin change failed!");
      return;
    }
  }
#endif

  if (!SD_MMC.begin("/sdcard", use_1_bit_mode)) {
    Serial.println("Card Mount Failed.");
    Serial.println("Increase log level to see more info: Tools > Core Debug Level > Verbose");
    Serial.println("Make sure that all data pins have a 10k Ohm pull-up resistor to 3.3V");
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    Serial.println("Make sure that when using generic ESP32-S3 board the pins are setup using SD_MMC.setPins()");
#endif
    return;
  }
  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  listDir(SD_MMC, "/", 0);
  removeDir(SD_MMC, "/mydir");
  createDir(SD_MMC, "/mydir");
  deleteFile(SD_MMC, "/hello.txt");
  writeFile(SD_MMC, "/hello.txt", "Hello ");
  appendFile(SD_MMC, "/hello.txt", "World!\n");
  listDir(SD_MMC, "/", 0);
}

void loop() {}
