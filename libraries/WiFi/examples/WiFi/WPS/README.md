Example Serial Logs For Various Cases
======================================

For WPS Push Button method,after the ESP32 boots up and prints that WPS has started, press the button that looks something like [this](https://www.verizon.com/supportresources/images/fqgrouter-frontview-wps-button.png) on your router. In case you dont find anything similar, check your router specs if it does really support WPS Push functionality.

As for WPS Pin Mode, it will output a 8 digit Pin on the Serial Monitor that will change every 2 minutes if it hasn't connected. You need to log in to your router (generally reaching 192.168.0.1) and enter the pin shown in Serial Monitor in the WPS Settings of your router.

#### WPS Push Button Failure

```
ets Jun  8 2016 00:22:57

rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0010,len:4
load:0x3fff0014,len:732
load:0x40078000,len:0
load:0x40078000,len:11572
entry 0x40078a14

Starting WPS
Station Mode Started
WPS Timedout, retrying
WPS Timedout, retrying
```

#### WPS Push Button Successfull

```
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
ets Jun  8 2016 00:22:57

rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0010,len:4
load:0x3fff0014,len:732
load:0x40078000,len:0
load:0x40078000,len:11572
entry 0x40078a14

Starting WPS
Station Mode Started
WPS Successfull, stopping WPS and connecting to: < Your Router SSID >
Disconnected from station, attempting reconnection
Connected to : < Your Router SSID >
Got IP: 192.168.1.100
```

#### WPS PIN Failure

```
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
ets Jun  8 2016 00:22:57

rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0010,len:4
load:0x3fff0014,len:732
load:0x40078000,len:0
load:0x40078000,len:11572
entry 0x40078a14

Starting WPS
Station Mode Started
WPS_PIN = 94842104
WPS Timedout, retrying
WPS_PIN = 55814171
WPS Timedout, retrying
WPS_PIN = 71321622
```

#### WPS PIN Successfull

```
ets Jun  8 2016 00:22:57

rst:0x10 (RTCWDT_RTC_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0010,len:4
load:0x3fff0014,len:732
load:0x40078000,len:0
load:0x40078000,len:11572
entry 0x40078a14

Starting WPS
Station Mode Started
WPS_PIN = 36807581
WPS Successfull, stopping WPS and connecting to: <Your Router SSID>
Disconnected from station, attempting reconnection
Connected to :<Your Router SSID>
Got IP: 192.168.1.100
```
