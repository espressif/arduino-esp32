Arduino core for ESP32 WiFi chip
===========================================

### Installation
- Install Arduino 1.6.9
- Go to Arduino directory
- Clone this repository into hardware/espressif/esp32 directory (or clone it elsewhere and create a symlink)
```bash
cd hardware
mkdir espressif
cd espressif
git clone https://github.com/espressif/arduino-esp32.git esp32
```
- Download binary tools (you need Python 2.7)
```bash
cd esp32/tools
python get.py
```
- Restart Arduino

![Pin Functions](doc/esp32_pinmap.png)
