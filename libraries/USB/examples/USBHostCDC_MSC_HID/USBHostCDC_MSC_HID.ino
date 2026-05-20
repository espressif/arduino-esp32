/*
 * Example: USB host — serial (CDC) + flash drive (MSC) + mouse and keyboard (HID)
 *
 * What you need:
 *   - ESP32-S2, S3, or P4 with USB OTG, and Arduino "USB Mode" set for **USB host** (often "Hardware CDC").
 *   - Call USBHost.begin() once, then USBHost.task() often (here: every loop).
 *
 * What this sketch does:
 *   - **CDC:** bytes from a USB serial device appear on the Serial Monitor; typing in the Monitor is sent to the device.
 *   - **MSC:** when a USB flash drive is plugged in, it mounts as a FAT disk (same idea as an SD card).
 *   - **HID:** prints simple lines when a USB mouse or boot keyboard sends data.
 *     (Some mice send reports very often; that is normal.)
 *
 * Like TinyUSB’s host demo "cdc_msc_hid", but using the Arduino USB library classes.
 */

#include <Arduino.h>

#if !SOC_USB_OTG_SUPPORTED
#error Use a board with USB OTG (ESP32-S2 / S3 / P4).
#else

#include <USBHost.h>
#include <USBHostSerial.h>
#include <USBHostMSC.h>
#include <USBMSCFS.h>
#include <FS.h>
#include <USBHostHID.h>
#include <USBHostHIDMouse.h>
#include <USBHostHIDKeyboard.h>
#include <USBHostHIDKeyboardDecode.h>

static const unsigned long kHostSerialBaud = 115200;
static const char *kUsbMountPath = "/usb";

static bool s_usb_stick_mounted = false;
static bool s_printed_cdc_ready = false;

static void onKeyboardReport(uint8_t modifiers, const uint8_t keys[6], void *) {
  bool any = modifiers != 0;
  for (int i = 0; i < 6 && !any; i++) {
    any = (keys[i] != 0);
  }
  if (!any) {
    USBHostKeyboard.clear();
    return;
  }

  char ascii[8];
  usbHostHidBootReportAppendAscii(ascii, sizeof(ascii), modifiers, keys, KeyboardLayout_en_US);

  Serial.print(F("[keyboard] "));
  if (ascii[0]) {
    Serial.print(ascii);
  } else {
    Serial.print(F("(non-printable keys)"));
  }
  Serial.println();

  USBHostKeyboard.clear();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("=== USB host: CDC + MSC + HID (starter example) ==="));
  Serial.println(F("Plug a USB serial adapter, a FAT flash drive, and/or a mouse or keyboard (hub is OK)."));
  Serial.println();

  USBHostMouse.registerWithHost();
  USBHostKeyboard.registerWithHost();
  USBHostKeyboard.setNotifyOnChangeOnly(true);
  USBHostKeyboard.setReportCallback(onKeyboardReport, nullptr);

#if defined(USB_HOST_EN) && defined(DEV_VBUS_EN)
  usbHostEnable(true);
  delay(10);
  usbHostPower(USB_HOST_POWER_VBUS);
  delay(10);
#endif

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
    if (!s_usb_stick_mounted) {
      if (USBMSCFS.begin(kUsbMountPath, 10, false)) {
        s_usb_stick_mounted = true;
        unsigned long mb = (unsigned long)(USBMSCFS.cardSize() / (1024UL * 1024UL));
        Serial.print(F("[msc] USB drive ready. Size ~ "));
        Serial.print(mb);
        Serial.print(F(" MB. Mount path: "));
        Serial.print(kUsbMountPath);
        Serial.println(F(" — use the File API like SD (open/read/write on USBMSCFS)."));
      }
    }
  } else {
    if (s_usb_stick_mounted) {
      USBMSCFS.end();
      s_usb_stick_mounted = false;
      Serial.println(F("[msc] USB drive removed."));
    }
  }

  // --- HID: mouse (simple print) ---
  if (USBHostMouse.mounted() && USBHostMouse.available()) {
    Serial.print(F("[mouse] dx="));
    Serial.print((int)USBHostMouse.getX());
    Serial.print(F(" dy="));
    Serial.print((int)USBHostMouse.getY());
    Serial.print(F(" buttons=0x"));
    Serial.print(USBHostMouse.getButtons(), HEX);
    Serial.print(F(" wheel="));
    Serial.println((int)USBHostMouse.getWheel());
    USBHostMouse.clear();
  }

  delay(2);
}

#endif /* host build */
