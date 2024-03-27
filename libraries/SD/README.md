# SD library

This library provides the integration of ESP32 and SD (Secure Digital) and MMC (Multi Media Card) cards without additional modules. This library is using SPI to interface with the cards. Please note that SPI mode is slower than the intended SD or MMC mode, however, provides more flexibility as the SPI module is available on all ESP SoCs and can be routed to any GPIO through GPIO matrix.

## Sample wiring diagram:

![Connections](http://i.imgur.com/4CoXOuR.png)

For other SD formats:

![Other SD card formats](https://upload.wikimedia.org/wikipedia/commons/thumb/a/ab/MMC-SD-miniSD-microSD-Color-Numbers-Names.gif/330px-MMC-SD-miniSD-microSD-Color-Numbers-Names.gif)

Image source: [Wikipedia](https://upload.wikimedia.org/wikipedia/commons/thumb/a/ab/MMC-SD-miniSD-microSD-Color-Numbers-Names.gif/330px-MMC-SD-miniSD-microSD-Color-Numbers-Names.gif)

> **Warning**
Some ESP32 modules have different pin outs!

## Default SPI pins:
Note that SPI pins can be configured by using `SPI.begin(sck, miso, mosi, cs);` alternatively, you can change only the CS pin with `SD.begin(CSpin)`

| SPI Pin Name | ESP8266 | ESP32 | ESP32‑S2 | ESP32‑S3 | ESP32‑C3 | ESP32‑C6 | ESP32‑H2 |
|--------------|---------|-------|----------|----------|----------|----------|----------|
| CS (SS)      | GPIO15  | GPIO5 | GPIO34   | GPIO10   | GPIO7    | GPIO18   | GPIO0    |
| DI (MOSI)    | GPIO13  | GPIO23| GPIO35   | GPIO11   | GPIO6    | GPIO19   | GPIO25    |
| DO (MISO)    | GPIO12  | GPIO19| GPIO37   | GPIO13   | GPIO5    | GPIO20   | GPIO11    |
| SCK (SCLK)   | GPIO14  | GPIO18| GPIO36   | GPIO12   | GPIO4    | GPIO21   | GPIO10    |

## FAQ:

**Do I need any additional modules**, like **the **Arduino**** SD module**?**

No, just wire your SD card directly to ESP32.

Tip: If you are using a microSD card and have a spare adapter to full-sized SD, you can solder Dupont pins on the adapter.


**What is the difference between SD and SD_MMC libraries?**

SD runs on SPI, and SD_MMC uses the SDMMC hardware bus on the ESP32.
The SPI uses 4 communication pins + 2 power connections and operates on up to 80MHz. The SPI option offers flexibility on pin connection because the data connections can be routed through GPIO matrix to any data pin.
SD-SPI speed is approximately half of the SD-MMC even when used on 1-bit line.
You can read more about SD SPI in the [documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html)

SD_MMC is supported only by ESP32 and ESP32-S3 and can be connected only to dedicated pins. SD_MMC allows to use of 1, 4 or 8 data pins + 2 additional communication pins and 2 power pins. The data pins need to be pulled up externally.
You can read more about SD_MMC in the [documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html)
1-bit: SD_MMC_ speed is approximately two-times faster than SPI mode
4-bit: SD_MMC speed is approximately three-times faster than SPI mode.
