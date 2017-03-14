
# SD library

This library provides the integration of ESP32 and SD (Secure Digital) cards without additional modules.


## Sample wiring diagram:


![SD card pins](http://i.imgur.com/4CoXOuR.png)

For others SD formats:


![Other SD card formats](https://upload.wikimedia.org/wikipedia/commons/thumb/a/ab/MMC-SD-miniSD-microSD-Color-Numbers-Names.gif/330px-MMC-SD-miniSD-microSD-Color-Numbers-Names.gif)


Image source: [Wikipedia](https://upload.wikimedia.org/wikipedia/commons/thumb/a/ab/MMC-SD-miniSD-microSD-Color-Numbers-Names.gif/330px-MMC-SD-miniSD-microSD-Color-Numbers-Names.gif)

```diff
- Warning: Some ESP32 modules have different pinouts!
```



## FAQ:

**Do I need any additional modules, like Arduino SD module?**

No, just wire your SD card directly to ESP32.



**What is the difference between SD and SD_MMC libraries?**

SD runs on SPI, and SD_MMC uses the SDMMC hardware bus on the ESP32.



**Can I change the CS pin?**

Yes, just use: `SD.begin(CSpin)`
