/*
 * Example: USB host — serial (CDC) + flash drive (MSC) + mouse and keyboard (HID)
 *
 * What you need:
 *   - ESP32-S2, S3, or P4 with USB OTG; disable CDC/MSC/DFU on boot when using USB-OTG mode.
 *   - Call USBHost.begin() once, then USBHost.task() often (here: every loop).
 *
 * What this sketch does:
 *   - **CDC:** bytes from a USB serial device appear on the Serial Monitor; typing in the Monitor is sent to the device.
 *   - **MSC:** when a USB flash drive is plugged in, it mounts as a FAT disk (same idea as an SD card).
 *   - **HID:** mouse and keyboard report callbacks print a line on each report.
 *     (Some mice send reports very often; that is normal.)
 *
 * Like TinyUSB’s host demo "cdc_msc_hid", but using the Arduino USB library classes.
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostSerial.h>
#include <USBHostMSC.h>
#include <USBHostHID.h>
#include <USBHostHIDMouse.h>
#include <USBHostHIDKeyboard.h>

static const unsigned long kHostSerialBaud = 115200;
static const char *kUsbMountPath = "/usb";

static bool s_usb_stick_mounted = false;
static bool s_usb_mount_attempted = false;
static bool s_printed_cdc_ready = false;

static void listUsbRoot() {
  /* Paths are volume-relative: mount is at /usb in VFS, but open("/") not open("/usb"). */
  File root = USBMSCFS.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println(F("[msc] open(\"/\") failed"));
    return;
  }
  Serial.println(F("[msc] root listing:"));
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    Serial.print(f.isDirectory() ? F("  DIR  ") : F("  FILE "));
    Serial.print(f.name());
    if (!f.isDirectory()) {
      Serial.print(F("  "));
      Serial.print(f.size());
    }
    Serial.println();
    f.close();
  }
  root.close();
}

static void onMouseReport(int16_t x, int16_t y, uint8_t buttons, int8_t wheel, void *) {
  Serial.print(F("[mouse] dx="));
  Serial.print((int)x);
  Serial.print(F(" dy="));
  Serial.print((int)y);
  Serial.print(F(" buttons=0x"));
  Serial.print(buttons, HEX);
  Serial.print(F(" wheel="));
  Serial.println((int)wheel);
  USBHostMouse.clear();
}

static void onKeyboardReport(uint8_t modifiers, const uint8_t keys[6], void *) {
  bool any = modifiers != 0;
  for (int i = 0; i < 6 && !any; i++) {
    any = (keys[i] != 0);
  }
  if (!any) {
    USBHostKeyboard.clear();
    return;
  }

  Serial.print(F("[keyboard] "));
  USBHostKeyboard.printReport(Serial, modifiers, keys);
  Serial.println();
  USBHostKeyboard.clear();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.printf(
    "build flags: USB_MODE=%d CDC_ON_BOOT=%d MSC_ON_BOOT=%d DFU_ON_BOOT=%d\n", (int)ARDUINO_USB_MODE, (int)ARDUINO_USB_CDC_ON_BOOT,
    (int)ARDUINO_USB_MSC_ON_BOOT, (int)ARDUINO_USB_DFU_ON_BOOT
  );
  Serial.println(F("(Host: in USB-OTG mode set all *_ON_BOOT=0; use ESP-Prog/UART or Hardware CDC for Serial Monitor.)"));
  Serial.println();

  Serial.println(F("=== USB host: CDC + MSC + HID (starter example) ==="));
  Serial.println(F("Plug a USB serial adapter, a FAT flash drive, and/or a mouse or keyboard (hub is OK)."));
  Serial.println();

  USBHostMouse.registerWithHost();
  USBHostMouse.setReportCallback(onMouseReport, nullptr);
  USBHostKeyboard.registerWithHost();
  USBHostKeyboard.setNotifyOnChangeOnly(true);
  USBHostKeyboard.setReportCallback(onKeyboardReport, nullptr);

  if (!USBHost.begin()) {
    Serial.println(F("USBHost.begin() failed."));
    return;
  }

  USBHostSerial.begin(kHostSerialBaud);

  Serial.println(F("Host started. Open the Serial Monitor and interact with USB devices."));
}

void loop() {
  USBHost.task();

  // --- CDC: USB serial <-> Arduino Serial Monitor ---
  if (USBHostSerial) {
    if (!s_printed_cdc_ready) {
      s_printed_cdc_ready = true;
      Serial.println(F("[cdc] USB serial device connected."));
    }
    uint8_t buf[64];
    size_t n = USBHostSerial.read(buf, sizeof(buf));
    if (n) {
      Serial.write(buf, n);
    }
    while (Serial.available()) {
      int c = Serial.read();
      if (c < 0) {
        break;
      }
      USBHostSerial.write((uint8_t)c);
    }
    USBHostSerial.flush();
  } else {
    s_printed_cdc_ready = false;
  }

  // --- MSC: USB flash drive as a FAT volume ---
  if (USBHostMSC.mounted()) {
    if (!s_usb_stick_mounted && !s_usb_mount_attempted) {
      s_usb_mount_attempted = true;
      if (USBMSCFS.begin(kUsbMountPath, 10, false)) {
        s_usb_stick_mounted = true;
        unsigned long mb = (unsigned long)(USBMSCFS.cardSize() / (1024UL * 1024UL));
        Serial.print(F("[msc] USB drive ready. Size ~ "));
        Serial.print(mb);
        Serial.print(F(" MB. Mount path: "));
        Serial.print(kUsbMountPath);
        Serial.println(F(" — use the File API like SD (open/read/write on USBMSCFS)."));
        listUsbRoot();
      } else {
        Serial.println(F("[msc] USBMSCFS.begin() failed (need FAT16/FAT32; see log_e). Unplug/replug to retry."));
      }
    }
  } else {
    s_usb_mount_attempted = false;
    if (s_usb_stick_mounted) {
      USBMSCFS.end();
      s_usb_stick_mounted = false;
      Serial.println(F("[msc] USB drive removed."));
    }
  }

  delay(2);
}
