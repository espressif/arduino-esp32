# SD_MMC library

This library provides the integration of ESP32 and ESP32-S3 with SD (Secure Digital) and MMC (Multi Media Card) cards using a built-in SDMMC module.

Please note that SD_MMC is only available for ESP32 and ESP32-S3. For other SoCs please use the SD library based on SPI.

## Wiring:

![MMC pins](https://commons.wikimedia.org/wiki/File:15-04-29-MMC-Karte-dscf4734-e.jpg)

Image source: [Wikipedia](https://commons.wikimedia.org/wiki/File:15-04-29-MMC-Karte-dscf4734-e.jpg)

### Pin assignments for ESP32

On ESP32, SD_MMC peripheral is connected to specific GPIO pins and cannot be changed (rerouted). Please see the table below for the pin connections.

When using an ESP-WROVER-KIT board, this example runs without any extra modifications required. Only an SD card needs to be inserted into the slot.

ESP32 pin     | SD card pin | Notes
--------------|-------------|------------
GPIO14 (MTMS) | CLK         | 10k pullup in SD mode
GPIO15 (MTDO) | CMD         | 10k pullup in SD mode
GPIO2         | D0          | 10k pullup in SD mode, pull low to go into download mode (see Note about GPIO2 below!)
GPIO4         | D1          | not used in 1-line SD mode; 10k pullup in 4-line SD mode
GPIO12 (MTDI) | D2          | not used in 1-line SD mode; 10k pullup in 4-line SD mode (see Note about GPIO12 below!)
GPIO13 (MTCK) | D3          | not used in 1-line SD mode, but card's D3 pin must have a 10k pullup


### Pin assignments for ESP32-S3

On ESP32-S3, SDMMC peripheral is connected to GPIO pins using GPIO matrix. This allows arbitrary GPIOs to be used to connect an SD card or MMC. The GPIOs can be configured based on
```
  setPins(int clk, int cmd, int d0))
  setPins(int clk, int cmd, int d0, int d1, int d2, int d3))
```

The table below lists the default pin assignments.

When using an ESP32-S3-USB-OTG board, this example runs without any extra modifications required. Only an SD card needs to be inserted into the slot.

ESP32-S3 pin  | SD card pin | Notes
--------------|-------------|------------
GPIO36        | CLK         | 10k pullup
GPIO35        | CMD         | 10k pullup
GPIO37        | D0          | 10k pullup
GPIO38        | D1          | not used in 1-line SD mode; 10k pullup in 4-line mode
GPIO33        | D2          | not used in 1-line SD mode; 10k pullup in 4-line mode
GPIO34        | D3          | not used in 1-line SD mode, but card's D3 pin must have a 10k pullup

### 4-line and 1-line SD modes

By default, this library uses 4-bit line mode, utilizing 6 pins: CLK, CMD, D0 - D3 and 2 power lines (3.3V and GND). It is possible to use 1-bit line mode (CLK, CMD, D0, 3.3V, GND) by passing the second argument `mode1bit==true`:
```
  SD_MMC.begin("/sdcard", true);
```

Note that even if card's D3 line is not connected to the ESP chip, it still has to be pulled up, otherwise the card will go into SPI protocol mode.

### Note about GPIO2 (ESP32 only)

GPIO2 pin is used as a bootstrapping pin, and should be low to enter UART download mode. One way to do this is to connect GPIO0 and GPIO2 using a jumper, and then the auto-reset circuit on most development boards will pull GPIO2 low along with GPIO0, when entering download mode.

- Some boards have pulldown and/or LED on GPIO2. LED is usually ok, but pulldown will interfere with D0 signals and must be removed. Check the schematic of your development board for anything connected to GPIO2.

### Note about GPIO12 (ESP32 only)

GPIO12 is used as a bootstrapping pin to select output voltage of an internal regulator which powers the flash chip (VDD_SDIO). This pin has an internal pulldown so if left unconnected it will read low at reset (selecting default 3.3V operation). When adding a pullup to this pin for SD card operation, consider the following:

## FAQ:

**Do I need any additional modules**, like **the **Arduino**** SD module**?**

No, just wire your SD card directly to ESP32.

Tip: If you are using a microSD card and have a spare adapter to full-sized SD, you can solder Dupont pins on the adapter.


**What is the difference between SD and SD_MMC libraries?**

SD runs on SPI, and SD_MMC uses the SDMMC hardware bus on the ESP32.
The SPI uses 4 communication pins + 2 power connections and operates on up to 80MHz. The SPI option offers flexibility on pin connection because the data connections can be routed through GPIO matrix to any data pin.
SD-SPI speed is approximately half of the SD-MMC even when used on 1-bit line.
You can read more about SD SPI in the [documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html)

SD_MMC is supported only by ESP32 and ESP320S3 and can be connected only to dedicated pins. SD_MMC allows to use of 1, 4 or 8 data pins + 2 additional communication pins and 2 power pins. The data pins need to be pulled up externally.
You can read more about SD_MMC in the [documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdmmc_host.html)
1-bit: SD_MMC_ speed is approximately two-times faster than SPI mode
4-bit: SD_MMC speed is approximately three-times faster than SPI mode.
