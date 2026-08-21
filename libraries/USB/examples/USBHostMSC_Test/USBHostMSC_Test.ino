/*
 * USB Host MSC — filesystem test (based on SD/examples/SD_Test/SD_Test.ino)
 *
 * Requires ESP32-S2 / S3 / P4 with USB OTG and a FAT-formatted USB flash drive.
 * Wiring: connect the drive to the USB host port; on ESP32-S3-USB-OTG enable VBUS (sketch below).
 *
 * Flow: USBHost.begin() → wait for MSC → USBMSCFS.begin("/usb") → same File API tests as SD_Test.
 */

#include <Arduino.h>
#include <inttypes.h>
#include <USBHost.h>
#include <USBHostMSC.h>
#include <USBMSCFS.h>
#include <FS.h>

#ifndef USB_MSC_MOUNTPOINT
#define USB_MSC_MOUNTPOINT "/usb"
#endif

/** Wait up to this many ms for a USB MSC device to enumerate after host start. */
#ifndef USB_MSC_WAIT_MS
#define USB_MSC_WAIT_MS 60000
#endif

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
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
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

void testFileIO(fs::FS &fs, const char *path) {
  static uint8_t buf[512];
  Serial.print("Starting 1Mb write test...");

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  uint32_t start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  uint32_t end = millis() - start;
  Serial.printf("%u bytes written for %" PRIu32 " ms\n", 2048 * 512, end);
  file.close();

  Serial.print("Starting 1Mb read test...");
  file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  size_t len = file.size();
  size_t flen = len;
  start = millis();
  while (len) {
    size_t toRead = len;
    if (toRead > 512) {
      toRead = 512;
    }
    file.read(buf, toRead);
    len -= toRead;
  }
  end = millis() - start;
  Serial.printf("%lu bytes read for %" PRIu32 " ms\n", (unsigned long)flen, end);
  file.close();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("USB Host MSC Test (SD_Test-style)");

#if defined(USB_HOST_EN) && defined(DEV_VBUS_EN)
  usbHostEnable(true);
  delay(10);
  usbHostPower(USB_HOST_POWER_VBUS);
  delay(10);
#endif

  if (!USBHost.begin()) {
    Serial.println("USBHost.begin() failed");
    return;
  }

  Serial.println("Waiting for USB Mass Storage device...");
  const uint32_t wait0 = millis();
  while (!USBHostMSC.mounted()) {
    USBHost.task();
    yield();
    if ((millis() - wait0) > (uint32_t)USB_MSC_WAIT_MS) {
      Serial.println("Timeout: no MSC device. Plug a FAT USB stick and reset.");
      return;
    }
    delay(10);
  }

  if (!USBMSCFS.begin(USB_MSC_MOUNTPOINT, 10, false)) {
    Serial.println("USBMSCFS.begin / FAT mount failed");
    return;
  }

  /* Verbose USB MSC traces: Tools -> Core Debug Level -> Verbose (log_v / log_buf_v). */

  Serial.print("USB MSC: ");
  Serial.print(USBMSCFS.cardSize() / (1024 * 1024));
  Serial.print(" MB, block ");
  Serial.print(USBMSCFS.sectorSize());
  Serial.print(" B, ");
  Serial.print((unsigned)USBMSCFS.numSectors());
  Serial.println(" sectors");

  listDir(USBMSCFS, "/", 0);
  createDir(USBMSCFS, "/mydir");
  listDir(USBMSCFS, "/", 0);
  removeDir(USBMSCFS, "/mydir");
  listDir(USBMSCFS, "/", 2);
  writeFile(USBMSCFS, "/hello.txt", "Hello ");
  appendFile(USBMSCFS, "/hello.txt", "World!\n");
  readFile(USBMSCFS, "/hello.txt");
  deleteFile(USBMSCFS, "/foo.txt");
  renameFile(USBMSCFS, "/hello.txt", "/foo.txt");
  readFile(USBMSCFS, "/foo.txt");
  testFileIO(USBMSCFS, "/test.txt");

  Serial.print("Total space: ");
  Serial.print(USBMSCFS.totalBytes() / (1024 * 1024));
  Serial.println("MB");
  Serial.print("Used space: ");
  Serial.print(USBMSCFS.usedBytes() / (1024 * 1024));
  Serial.println("MB");

  USBMSCFS.end();
  Serial.println("Tests done. USBMSCFS.end() — safe to unplug or reset.");
}

void loop() {
  USBHost.task();
  delay(50);
}

