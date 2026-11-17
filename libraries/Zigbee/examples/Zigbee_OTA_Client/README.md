# Arduino-ESP32 Zigbee OTA Client + on/off light Example

This example shows how to configure the Zigbee end device with OTA Client and use it as a Home Automation (HA) on/off light.

The client supports **full** Zigbee OTA images (not Delta OTA). Use a dual-app partition scheme so the new firmware can be written beside the running image.

# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C5 | ESP32-C6 | ESP32-H2 | ESP32-S31 |
| ----------------- | -------- | -------- | -------- | --------- |

## Hardware Required

* One ESP32-C5 / C6 / H2 / S31 board running this example (OTA client + on/off light)
* A Zigbee coordinator that provides an OTA Upgrade server, for example:
  * Home Assistant with [ZHA](https://www.home-assistant.io/integrations/zha/)
  * Or the ESP Zigbee SDK [`ota_server`](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/ota_upgrade/ota_server) example
* A USB cable for power supply and programming

### Configure the Project

Set the LED GPIO by changing the `LED_PIN` definition. By default, the LED_PIN is `RGB_BUILTIN`.
By default, the `rgbLedWrite` function is used to control the LED. You can change it to digitalWrite to control a simple LED.

OTA identifiers used by this sketch (must match the OTA image and the coordinator):

* `OTA_UPGRADE_MANUFACTURER` (`0x1001`)
* `OTA_UPGRADE_IMAGE_TYPE` (`0x1011`)
* `OTA_UPGRADE_RUNNING_FILE_VERSION` — version of this firmware (also used as the OTA file version on Export)
* `OTA_UPGRADE_DOWNLOADED_FILE_VERSION` — version already stored on the client (`0xFFFFFFFF` = none). Do **not** set this to the next OTA file version; the stack rejects an offered image that matches this attribute and never starts the download.
* `OTA_UPGRADE_HW_VERSION` — optional hardware version filter in the OTA header

Bump `OTA_UPGRADE_RUNNING_FILE_VERSION` for each new image you export for the coordinator. Leave `OTA_UPGRADE_DOWNLOADED_FILE_VERSION` at `0xFFFFFFFF` unless you intentionally track a stored image.

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* Optional: Set debug level to verbose to see all logs from Zigbee stack: `Tools -> Core Debug Level: Verbose`.

## OTA with Home Assistant ZHA

**Version rule:** the `.zigbee` file version must be **greater than** the version running on the device. ZHA returns `NO_IMAGE_AVAILABLE` (0x98) if the file is missing, unreadable, or not newer.

**Firmware status in ZHA:** until you place any matching `.zigbee` file in the Home Assistant OTA folder (`zigpy_ota`), ZHA shows the device firmware as **Unknown** — it has nothing to compare against. After you upload an image with the **same** version as the currently flashed firmware, ZHA can show **Up-to-date**. Upload a **newer** version when you want an Update available.

Typical first test:

1. Flash this example via USB with `OTA_UPGRADE_RUNNING_FILE_VERSION` `0x01010100`, `OTA_UPGRADE_DOWNLOADED_FILE_VERSION` `0xFFFFFFFF`, and join the device to ZHA. Serial should print `Running FW version: 0x01010100`.
2. **Do not USB-flash the next build.** Only bump `OTA_UPGRADE_RUNNING_FILE_VERSION` → `0x01010101` for the update image (this becomes the OTA file version on Export). Keep `OTA_UPGRADE_DOWNLOADED_FILE_VERSION` as `0xFFFFFFFF`.
3. Keep **Tools → Zigbee Mode** enabled (ED), then **Sketch → Export compiled Binary**.
4. The core runs `tools/zb_ota_image.py` and writes a Zigbee OTA file next to the `.bin`, for example:

   `sketch/build/<fqbn>/1001-1011-01010101-ota-file.zigbee`

   It reads the sketch `#define`s above (literal integers only). Soft-skips when Zigbee Mode is Disabled, the sketch has no OTA intent, or required macros are missing — Export still succeeds.

   Manual alternative: Espressif's [image_builder_tool](https://github.com/espressif/esp-zigbee-sdk/tree/main/tools/image_builder_tool) (same manufacturer, image type, and **higher** file version):

```bash
python image_builder_tool.py --manuf-id 0x1001 --image-type 0x1011 \
  --file-version 0x01010101 --tag 0x0000:firmware.bin
```

See also: [ESP Zigbee SDK — Generate and apply OTA upgrade image](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/user-guide/zcl_ota_upgrade.html).

5. On Home Assistant, install the **Samba share** add-on (Settings → Add-ons → Add-on store), set a username/password, and start it.
6. From Windows open `\\<HA_IP>\`, go to the `config` share, create a `zigpy_ota` folder, and copy the **newer** `.zigbee` file into it (filename should contain the higher version, e.g. `...-01010101-...`).
7. Add Zigpy OTA configuration to `configuration.yaml`. The `advanced` provider requires the exact `warning` text below (do not paraphrase — zigpy rejects any other string):

```yaml
zha:
  zigpy_config:
    ota:
      extra_providers:
        - type: advanced
          path: /config/zigpy_ota
          warning: >-
            I understand I can *destroy* my devices by enabling OTA updates from files.
            Some OTA updates can be mistakenly applied to the wrong device, breaking it.
            I am consciously using this at my own risk.
```

8. After copying (or replacing) the `.zigbee` file into `/config/zigpy_ota`, open **Settings → Devices & services → Zigbee Home Automation**, click the **⋮** (three dots) on the ZHA integration card, and choose **Reload**. This reloads the OTA provider so ZHA picks up the new file (a full Home Assistant restart also works).
9. Open the end device in ZHA and click **Update**. Wait until the transfer finishes and the board reboots; Serial should then print the new version (e.g. `0x01010101`).

`requestOTAUpdate()` after join discovers the OTA server on the coordinator and sends Query Next Image (then every hour). ZHA can also start the flow with Image Notify when you press Update.

If ZHA still reports `NO_IMAGE_AVAILABLE`, check: file is under `/config/zigpy_ota`, filename version is higher than the running FW, manufacturer `0x1001` / image type `0x1011` match the sketch, and Home Assistant was restarted after copying the file.

## Troubleshooting

If the End device flashed with this example is not connecting to the coordinator, erase the flash of the End device before flashing the example to the board. It is recommended to do this if you re-flash the coordinator.
You can do the following:

* In the Arduino IDE go to the Tools menu and set `Erase All Flash Before Sketch Upload` to `Enabled`.
* Add to the sketch `Zigbee.factoryReset();` to reset the device and Zigbee stack.

By default, the coordinator network is closed after rebooting or flashing new firmware.
To open the network you have 2 options:

* Open network after reboot by setting `Zigbee.setRebootOpenNetwork(time);` before calling `Zigbee.begin();`.
* In application you can anytime call `Zigbee.openNetwork(time);` to open the network for devices to join.

***Important: Make sure you are using a good quality USB cable and that you have a reliable power source***

* **LED not blinking:** Check the wiring connection and the IO selection.
* **Programming Fail:** If the programming/flash procedure fails, try reducing the serial connection speed.
* **COM port not detected:** Check the USB cable and the USB to Serial driver installation.
* **OTA does not start:** Confirm manufacturer / image type / file version match the OTA file, the partition scheme is Zigbee with OTA slots, and ZHA can see the OTA provider path. If Serial shows `Accepting OTA image for download` but never `OTA upgrade start`, check that `OTA_UPGRADE_DOWNLOADED_FILE_VERSION` is `0xFFFFFFFF` (or at least not equal to the offered image version).

If the error persists, you can ask for help at the official [ESP32 forum](https://esp32.com) or see [Contribute](#contribute).

## Contribute

To know how to contribute to this project, see [How to contribute.](https://github.com/espressif/arduino-esp32/blob/master/CONTRIBUTING.rst)

If you have any **feedback** or **issue** to report on this example/library, please open an issue or fix it by creating a new PR. Contributions are more than welcome!

Before creating a new issue, be sure to try Troubleshooting and check if the same issue was already created by someone else.

## Resources

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32-C5 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c5_datasheet_en.pdf)
* ESP32-C6 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
* ESP32-H2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf)
* ESP32-S31 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s31_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
* [ESP Zigbee SDK OTA Upgrade Cluster](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/user-guide/zcl_ota_upgrade.html)
