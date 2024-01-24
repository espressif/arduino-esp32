# ESP32 Libraries

arduino-esp32 includes libraries for Arduino compatibility along with some object wrappers around hardware specific devices.  Examples are included in the examples folder under each library folder.  The ESP32 includes additional examples which need no special drivers.

### ArduinoOTA
  Over The Air firmware update daemon.  Use espota.py to upload to the device.

### AsyncUDP
  Asynchronous task driven UDP datagram client/server

### BLE
  Bluetooth Low Energy v4.2 client/server framework

### BluetoothSerial
  Serial to Bluetooth redirection server\
  Note: This library depends on Bluetooth Classic which is only available for ESP32\
  (Bluetoothserial is **not available** for ESP32-S2, ESP32-C3, ESP32-S3).

### DNSServer
  A basic UDP DNS daemon (includes captive portal demo)

### EEPROM
  Arduino compatibility for EEPROM (using flash)

### ESP32
  Additional examples
  * AnalogOut
  * Camera
  * ChipID
  * DeepSleep
  * ESPNow
  * FreeRTOS
  * GPIO
  * HallSensor
  * I2S
  * MacAddress
  * ResetReason
  * RMT
  * Time
  * Timer
  * Touch

### ESPmDNS
  mDNS service advertising

### Ethernet
  Ethernet networking

### FFat
  FAT indexed filesystem on SPI flash

### FS
  Filesystem virtualization framework

### HTTPClient
  A simple HTTP client, compatible with WiFiClientSecure

### HTTPUpdate
  Download a firmware update from HTTPd and apply it using Update

### HTTPUpdateServer
  Upload a firmware for the update from HTTPd

### LittleFS
  LittleFS (File System)

### NetBIOS
  NetBIOS name advertiser

### Preferences
  Flash keystore using ESP32 NVS

### ESP RainMaker
  End-to-end platform by Espressif that enables Makers to realize their IoT ideas faster

### SD
  Secure Digital card filesystem using SPI access

### SD_MMC
  Secure Digital card filesystem using 4-lane access

### SimpleBLE
  Minimal BLE advertiser

### SPI
  Arduino compatible Serial Peripheral Interface driver (master only)

### SPIFFS
  SPI Flash Filesystem (see [spiffs-plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin) to upload to device)

### Ticker
  A timer to call functions on an interval

### Update
  Sketch Update using ESP32 OTA functionality

### USB
  Universal Serial Bus driver (device only)

### WebServer
  A simple HTTP daemon

### WiFi
  Arduino compatible WiFi driver (includes Ethernet driver)

### WiFiClientSecure
  Arduino compatible WiFi client object using embedded encryption

### Wire
  Arduino compatible I2C driver
