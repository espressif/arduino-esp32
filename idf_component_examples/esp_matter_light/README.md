| Supported Targets | ESP32-S3 | ESP32-C3 | ESP32-C6 |
| ----------------- | -------- | -------- | -------- |


# Managed Component Light

This example sets automatically the RGB LED GPIO and BOOT Button GPIO based on the default pin used by the selected Devkit Board.

This example creates a Color Temperature Light device using the esp_matter component downloaded from the [Espressif Component Registry](https://components.espressif.com/) instead of an extra component locally, so the example can work without setting up the esp-matter environment.

Read the [documentation](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html) for more information about building and flashing the firmware.

The code is based on the Arduino API and uses Arduino as an IDF Component.

## How to use it

Once the device runs for the first time, it must be commissioned to the Matter Fabric of the available Matter Environment.  
Possible Matter Environments are:  
- Amazon Alexa
- Google Home Assistant (*)
- Apple Home
- Open Source Home Assistant

(*) Google Home Assistant requires the user to set up a Matter Light using the [Google Home Developer Console](https://developers.home.google.com/codelabs/matter-device#2). It is necessary to create a Matter Light device with VID = 0xFFF1 and PID = 0x8000. Otherwise, the Light won't show up in the GHA APP. This action is necessary because the Firmware uses Testing credentials and Google requires the user to create the testing device before using it.

**There is no QR Code** to be used when the Smartphone APP wants to add the Matter Device.  
Please enter the code manually: `34970112332`

Each Devkit Board has a built-in LED that will be used as the Matter Light.  
The default setting for ESP32-S3 is pin 48, for ESP32-C3 and ESP32-C6, it is pin 8.  
The BOOT Button pin of ESP32-S3 is GPIO 0, by toher hand, the ESP32-C3 and ESP32-C6 use GPIO 9.  
Please change it in using the MenuConfig executing `idf.py menuconfig` and selecting `Menu->Light Matter Accessory` options.

## LED Status and Factory Mode

The WS2812b built-in LED will turn purple as soon as the device is flashed and runs for the first time.  
The purple color indicates that the Matter Accessory has not been commissioned yet.  
After using a Matter provider Smartphone APP to add a Matter device to your Home Application, it may turn orange to indicate that it has no Wi-Fi connection.

Once it connects to the Wi-Fi network, the LED will turn white to indicate that Matter is working and the device is connected to the Matter Environment.
Please note that Matter over Wi-Fi using an ESP32 device will connect to a 2.4 GHz Wi-Fi SSID, therefore the Commissioner APP Smartphone shall be connected to this SSID.

The Matter and Wi-Fi configuration will be stored in NVS to ensure that it will connect to the Matter Fabric and Wi-Fi Network again once it is reset.

The Matter Smartphone APP will control the light state (ON/OFF), temperature (Warm/Cold White), and brightness.

## On Board Light toggle button

The built-in BOOT button will toggle On/Off and replicate the new state to the Matter Environment, making it visible in the Matter Smartphone APP as well.

## Returning to the Factory State

Holding the BOOT button pressed for more than 10 seconds and then releasing it will erase all Matter and Wi-Fi configuration, forcing it to reset to factory state. After that, the device needs to be commissioned again.  
Previous setups done in the Smartphone APP won't work again; therefore, the virtual device shall be removed from the APP.

## Building the Application using Wi-Fi and Matter

Use ESP-IDF 5.1.4 from https://github.com/espressif/esp-idf/tree/release/v5.1  
This example has been tested with Arduino Core 3.0.4

The project will download all necessary components, including the Arduino Core.  
Execute this sequence:
 `<remove build folder> using linux rm command or Windows rmdir command`
 `idf.py set-target <SoC_Target>`
 `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig_file1;sdkconfig_file2;sdkconfig_fileX" -p <PORT> flash monitor`

Example for ESP32-S3/Linux | macOS:  
```
rm -rf build
idf.py set-target esp32s3
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -p /dev/ttyACM0 flash monitor
```
Example for ESP32-C3/Windows:  
```
rmdir /s/q build
idf.py set-target esp32c3
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" -p com3 flash monitor
```

It may be necessary to delete some folders and files  before running `idf.py`  
- Linux/macOS:
  ```
  rm -rf build managed_components sdkconfig dependencies.lock
  ```  
- Windows:
  ```
  rmdir /s/q build managed_components && del sdkconfig dependencies.lock
  ```

There is a configuration file for these SoC: esp32s3, esp32c3, esp32c6.
Those are the tested devices that have a WS2812 RGB LED and can run BLE, Wi-Fi and Matter.

In case it is necessary to change the Button Pin or the REG LED Pin, please use the `menuconfig`
`idf.py menuconfig` and change the Menu Option `Light Matter Accessory`

## Building the Application using OpenThread and Matter

This is possible with the ESP32-C6.
It is necessary to have a Thread Border Router in the Matter Environment.  
Check your Matter hardware provider.  
In order to build the application that will use Thread Networking instead of Wi-Fi, please execute:

Example for ESP32-C6/Linux | macOS:  
```
rm -rf build
idf.py set-target esp32c6
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.c6_thread" -p /dev/ttyACM0 flash monitor
```
Example for ESP32-C6/Windows:  
```
rmdir /s/q build
idf.py set-targt esp32c6
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.c6_thread" -p com3 flash monitor
```

It may be necessary to delete some folders and files before running `idf.py`  
- Linux/macOS
  ```
  rm -rf build managed_components sdkconfig dependencies.lock
  ```  
- Windows
  ```
  rmdir /s/q build managed_components && del sdkconfig dependencies.lock
  ```
