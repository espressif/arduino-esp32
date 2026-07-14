/*
 * BLE HID Composite Device Example -- New API
 *
 * Creates a BLE HID composite device that combines keyboard, mouse, and
 * consumer control (media keys) into a single BLE peripheral. The host OS
 * sees one Bluetooth device but receives input from three logical HID
 * functions, each identified by a unique Report ID.
 *
 * How composite HID works
 * -----------------------
 * A HID Report Descriptor tells the host what kind of data the device sends
 * and how to interpret each byte. In a composite device, a single descriptor
 * contains multiple top-level "Application Collections", one per function
 * (keyboard, mouse, media). Each collection is tagged with a Report ID so
 * the host can demultiplex incoming reports to the correct driver:
 *
 *   Report ID 1 --> Keyboard driver
 *   Report ID 2 --> Mouse / pointing-device driver
 *   Report ID 3 --> Consumer Control (media keys) driver
 *
 * When the device sends a report it prepends the report ID byte so the host
 * knows which collection the payload belongs to. Each BLECharacteristic
 * obtained from hid.inputReport(id) is bound to that report ID.
 *
 * Features:
 *   - Keyboard: 8 modifier bits + 1 reserved byte + 6 simultaneous key slots
 *   - Mouse:    3 buttons + relative X/Y movement + vertical scroll wheel
 *   - Consumer: volume up, volume down, mute, play/pause, next/prev track
 *   - Secure pairing with bonding ("Just Works")
 *   - Battery level reporting
 *   - Auto re-advertise on disconnect
 *
 * Usage:
 *   1. Upload to your ESP32
 *   2. Pair from Settings > Bluetooth on your PC / phone
 *   3. Watch Serial Monitor for the demo cycling through keyboard, mouse,
 *      and media key reports
 *
 * Licensed under the Apache License, Version 2.0
 */

#include <Arduino.h>
#include <cstring>
#include <BLE.h>
#include <HIDTypes.h>

// ---------------------------------------------------------------------------
// Report IDs -- each maps to a separate Application Collection in the
// descriptor below. The host uses these to route incoming reports to the
// correct input subsystem.
// ---------------------------------------------------------------------------
static const uint8_t REPORT_ID_KB = 1;        // Keyboard
static const uint8_t REPORT_ID_MOUSE = 2;     // Mouse
static const uint8_t REPORT_ID_CONSUMER = 3;  // Consumer Control (media)

// ---------------------------------------------------------------------------
// HID Report Descriptor (composite: keyboard + mouse + consumer control)
//
// HID descriptors are a sequence of "short items" that describe the data
// format the device will send. Each item is 1-3 bytes: a tag/type/size byte
// followed by 0-2 data bytes. The host parses this once at enumeration to
// learn how to decode every subsequent Input report.
//
// The macros (USAGE_PAGE, COLLECTION, HIDINPUT, etc.) are defined in
// HIDTypes.h. Each macro produces the HID short-item prefix byte; the
// parameter is the number of data bytes that follow (usually 1). See
// HIDTypes.h for a full explanation of the encoding.
//
// Key concepts used here:
//   Usage Page   -- broad category (Generic Desktop, Keyboard, Button, Consumer)
//   Usage        -- specific function within the page (Keyboard, Mouse, etc.)
//   Collection   -- groups related items; Application (0x01) = top-level device
//   Report ID    -- 1-byte tag prepended to each report for demultiplexing
//   Report Size  -- width of each field in bits
//   Report Count -- number of fields of that size
//   Logical Min/Max -- value range the host should expect
//   Input        -- describes a block of data sent device-to-host
//
// References:
//   USB HID Usage Tables 1.4  -- https://usb.org/document-library/hid-usage-tables-14
//   Device Class Definition for HID 1.11, Section 6.2.2
// ---------------------------------------------------------------------------
// clang-format off
static const uint8_t kReportMap[] = {

  // =========================================================================
  // Collection 1: Keyboard (Report ID 1)
  //
  // Standard boot-keyboard layout:
  //   Byte 0: Modifier bitmask (8 bits for LCtrl..RGUI)
  //   Byte 1: Reserved (constant padding required by boot protocol)
  //   Bytes 2-7: Up to 6 simultaneous key codes (Usage Page 0x07)
  // Total report size: 8 bytes (not counting the Report ID byte which the
  // BLE HID layer prepends automatically on the characteristic).
  // =========================================================================
  USAGE_PAGE(1),      0x01,  // Generic Desktop
  USAGE(1),           0x06,  // Keyboard
  COLLECTION(1),      0x01,  // Application
  REPORT_ID(1),       0x01,

  //-- Modifier keys: 8 individual bits for Ctrl/Shift/Alt/GUI (left + right)
  USAGE_PAGE(1),      0x07,  //   Keyboard/Keypad
  USAGE_MINIMUM(1),   0xE0,  //   Left Control
  USAGE_MAXIMUM(1),   0xE7,  //   Right GUI
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,  //   1 bit per modifier
  REPORT_COUNT(1),    0x08,  //   8 modifier keys
  HIDINPUT(1),        0x02,  //   Data, Variable, Absolute

  //-- Reserved byte (padding mandated by the HID boot keyboard spec)
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x08,  //   8 bits
  HIDINPUT(1),        0x01,  //   Constant -- host ignores this byte

  //-- Key array: up to 6 simultaneous key-press codes
  REPORT_COUNT(1),    0x06,  //   6 key slots
  REPORT_SIZE(1),     0x08,  //   8 bits per slot
  LOGICAL_MINIMUM(1), 0x00,  //   0 = no key
  LOGICAL_MAXIMUM(1), 0x65,  //   101 = Keyboard Application
  USAGE_PAGE(1),      0x07,  //   Keyboard/Keypad
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(1),   0x65,
  HIDINPUT(1),        0x00,  //   Data, Array
  END_COLLECTION(0),

  // =========================================================================
  // Collection 2: Mouse (Report ID 2)
  //
  // 3-button relative mouse with vertical scroll wheel:
  //   Bits 0-2: Buttons (left, right, middle)
  //   Bits 3-7: Padding (5 bits)
  //   Byte 1:   X displacement (signed, -127..127)
  //   Byte 2:   Y displacement (signed, -127..127)
  //   Byte 3:   Scroll wheel   (signed, -127..127)
  // Total report size: 4 bytes.
  // =========================================================================
  USAGE_PAGE(1),      0x01,  // Generic Desktop
  USAGE(1),           0x02,  // Mouse
  COLLECTION(1),      0x01,  // Application
  REPORT_ID(1),       0x02,
  USAGE(1),           0x01,  //   Pointer
  COLLECTION(1),      0x00,  //   Physical

  //-- Mouse buttons: 3 bits for left, right, middle
  USAGE_PAGE(1),      0x09,  //     Button
  USAGE_MINIMUM(1),   0x01,  //     Button 1 (left click)
  USAGE_MAXIMUM(1),   0x03,  //     Button 3 (middle click)
  LOGICAL_MINIMUM(1), 0x00,  //     0 = released
  LOGICAL_MAXIMUM(1), 0x01,  //     1 = pressed
  REPORT_SIZE(1),     0x01,  //     1 bit per button
  REPORT_COUNT(1),    0x03,  //     3 buttons
  HIDINPUT(1),        0x02,  //     Data, Variable, Absolute

  //-- Padding: 5 bits to fill out the button byte
  REPORT_SIZE(1),     0x05,  //     5 bits
  REPORT_COUNT(1),    0x01,
  HIDINPUT(1),        0x01,  //     Constant (padding)

  //-- X and Y relative movement
  USAGE_PAGE(1),      0x01,  //     Generic Desktop
  USAGE(1),           0x30,  //     X
  USAGE(1),           0x31,  //     Y
  LOGICAL_MINIMUM(1), 0x81,  //     -127
  LOGICAL_MAXIMUM(1), 0x7F,  //     127
  REPORT_SIZE(1),     0x08,  //     8 bits
  REPORT_COUNT(1),    0x02,  //     2 axes
  HIDINPUT(1),        0x06,  //     Data, Variable, Relative

  //-- Vertical scroll wheel
  USAGE(1),           0x38,  //     Wheel
  LOGICAL_MINIMUM(1), 0x81,  //     -127
  LOGICAL_MAXIMUM(1), 0x7F,  //     127
  REPORT_SIZE(1),     0x08,  //     8 bits
  REPORT_COUNT(1),    0x01,
  HIDINPUT(1),        0x06,  //     Data, Variable, Relative

  END_COLLECTION(0),         //   End Physical
  END_COLLECTION(0),         // End Mouse

  // =========================================================================
  // Collection 3: Consumer Control (Report ID 3)
  //
  // Consumer Control devices send media-key events using Usage Page 0x0C.
  // We define 6 buttons, each a single bit:
  //   Bit 0: Volume Up        (Usage 0xE9)
  //   Bit 1: Volume Down      (Usage 0xEA)
  //   Bit 2: Mute             (Usage 0xE2)
  //   Bit 3: Play/Pause       (Usage 0xCD)
  //   Bit 4: Next Track       (Usage 0xB5)
  //   Bit 5: Previous Track   (Usage 0xB6)
  //   Bits 6-7: Padding
  // Total report size: 1 byte.
  // =========================================================================
  USAGE_PAGE(1),      0x0C,  // Consumer
  USAGE(1),           0x01,  // Consumer Control
  COLLECTION(1),      0x01,  // Application
  REPORT_ID(1),       0x03,

  //-- 6 media buttons, one bit each
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,  //   1 bit per button
  REPORT_COUNT(1),    0x06,  //   6 buttons
  USAGE(1),           0xE9,  //   Volume Up
  USAGE(1),           0xEA,  //   Volume Down
  USAGE(1),           0xE2,  //   Mute
  USAGE(1),           0xCD,  //   Play/Pause
  USAGE(1),           0xB5,  //   Scan Next Track
  USAGE(1),           0xB6,  //   Scan Previous Track
  HIDINPUT(1),        0x02,  //   Data, Variable, Absolute

  //-- Padding: 2 bits to complete the byte
  REPORT_SIZE(1),     0x02,
  REPORT_COUNT(1),    0x01,
  HIDINPUT(1),        0x01,  //   Constant (padding)
  END_COLLECTION(0)
};
// clang-format on

// ---------------------------------------------------------------------------
// Packed report structs
//
// These mirror the binary layout described by the report descriptor above.
// Using packed structs avoids manual byte-packing and makes the code
// self-documenting: each field matches one descriptor entry.
// ---------------------------------------------------------------------------

struct __attribute__((packed)) KeyboardReport {
  uint8_t modifiers;  // Bitmask: bit 0=LCtrl, 1=LShift, 2=LAlt, 3=LGUI,
                      //          4=RCtrl, 5=RShift, 6=RAlt, 7=RGUI
  uint8_t reserved;   // Always 0 (required by boot keyboard spec)
  uint8_t keys[6];    // Up to 6 simultaneous HID key codes (0 = no key)
};

struct __attribute__((packed)) MouseReport {
  uint8_t buttons;  // Bit 0=left, 1=right, 2=middle (upper 5 bits unused)
  int8_t x;         // Relative X movement (-127..127), positive = right
  int8_t y;         // Relative Y movement (-127..127), positive = down
  int8_t wheel;     // Scroll wheel (-127..127), positive = scroll up
};

struct __attribute__((packed)) ConsumerReport {
  uint8_t buttons;  // Bit 0=VolUp, 1=VolDown, 2=Mute, 3=PlayPause,
                    //     4=NextTrack, 5=PrevTrack (bits 6-7 unused)
};

// Consumer button bit positions (match the order of Usage entries above)
static const uint8_t CONSUMER_VOLUME_UP = (1 << 0);
static const uint8_t CONSUMER_VOLUME_DOWN = (1 << 1);
static const uint8_t CONSUMER_MUTE = (1 << 2);
static const uint8_t CONSUMER_PLAY_PAUSE = (1 << 3);
static const uint8_t CONSUMER_NEXT_TRACK = (1 << 4);
static const uint8_t CONSUMER_PREV_TRACK = (1 << 5);

// One BLECharacteristic per report ID -- each is a separate GATT characteristic
// with its own Report Reference descriptor that tells the host which report ID
// it carries.
BLECharacteristic inputKeyboard;
BLECharacteristic inputMouse;
BLECharacteristic inputConsumer;

volatile bool deviceConnected = false;

// ---------------------------------------------------------------------------
// Report helpers
//
// Each function builds a packed struct matching the descriptor layout, writes
// it to the correct BLECharacteristic (bound to a specific Report ID), and
// calls notify() to push the data over the BLE connection.
// ---------------------------------------------------------------------------

void sendKeyboardReport(uint8_t modifiers, const uint8_t keys[6]) {
  KeyboardReport rpt;
  rpt.modifiers = modifiers;
  rpt.reserved = 0;
  memcpy(rpt.keys, keys, 6);
  inputKeyboard.setValue(reinterpret_cast<uint8_t *>(&rpt), sizeof(rpt));
  inputKeyboard.notify();
}

void releaseAllKeys() {
  KeyboardReport rpt;
  memset(&rpt, 0, sizeof(rpt));
  inputKeyboard.setValue(reinterpret_cast<uint8_t *>(&rpt), sizeof(rpt));
  inputKeyboard.notify();
}

void sendMouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  MouseReport rpt;
  rpt.buttons = buttons;
  rpt.x = x;
  rpt.y = y;
  rpt.wheel = wheel;
  inputMouse.setValue(reinterpret_cast<uint8_t *>(&rpt), sizeof(rpt));
  inputMouse.notify();
}

void sendConsumerReport(uint8_t buttons) {
  ConsumerReport rpt;
  rpt.buttons = buttons;
  inputConsumer.setValue(reinterpret_cast<uint8_t *>(&rpt), sizeof(rpt));
  inputConsumer.notify();
}

void releaseConsumer() {
  sendConsumerReport(0x00);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("BLE HID Composite Device");

  // BLE.begin() initializes the BLE stack, sets the device name that appears
  // during discovery, and returns a BTStatus. The explicit bool conversion
  // returns true on success (BTStatus::OK == 0).
  BTStatus status = BLE.begin("ESP32-Composite");
  if (!status) {
    Serial.printf("BLE init failed: %s\n", status.toString());
    while (true) {
      delay(1000);
    }
  }

  // -- Security configuration --
  // BLE HID requires an encrypted link. We use "Just Works" pairing
  // (NoInputNoOutput) which provides encryption without a PIN. Bonding
  // stores the keys so reconnections skip pairing.
  BLESecurity sec = BLE.getSecurity();
  sec.setIOCapability(BLESecurity::NoInputNoOutput);
  // Parameters: bonding=true (persist keys across reboots),
  //             MITM=false (no man-in-the-middle protection -- no PIN),
  //             SC=true (use LE Secure Connections for stronger encryption)
  sec.setAuthenticationMode(true, false, true);

  // -- Server and connection callbacks --
  BLEServer server = BLE.createServer();
  server.onConnect([](BLEServer, const BLEConnInfo &conn) {
    Serial.printf("Client connected: %s\n", conn.getAddress().toString().c_str());
    deviceConnected = true;
  });
  server.onDisconnect([](BLEServer, const BLEConnInfo &, uint8_t reason) {
    // 'reason' is a BLE HCI disconnect code. Common values:
    //   0x13 = Remote User Terminated Connection
    //   0x08 = Connection Timeout
    //   0x16 = Connection Terminated by Local Host
    Serial.printf("Client disconnected (HCI reason 0x%02X)\n", reason);
    deviceConnected = false;
    BLE.startAdvertising();
  });

  // -- HID Device setup --
  BLEHIDDevice hid(server);
  hid.manufacturer("Espressif");

  // PnP ID characteristic: lets the host identify the device vendor/product.
  //   0x02        = vendor ID source is USB-IF (vs. 0x01 = Bluetooth SIG)
  //   0x05AC      = vendor ID (Apple's USB vendor ID, used as example)
  //   0x820A      = product ID (arbitrary, identifies this product)
  //   0x0310      = product version 3.1.0 (BCD-encoded)
  hid.pnp(0x02, 0x05AC, 0x820A, 0x0310);

  // HID Information characteristic:
  //   0x00 = bCountryCode: 0 means "not localized" (most devices use this)
  //   0x01 = flags: bit 0 set = "normally connectable" (remote-wake capable)
  hid.hidInfo(0x00, 0x01);

  // Install the composite report descriptor. The host parses this once to
  // learn about all three device functions and their report formats.
  hid.reportMap(kReportMap, sizeof(kReportMap));

  hid.setBatteryLevel(100);

  // Create one input-report characteristic per Report ID. Each characteristic
  // has a Report Reference descriptor (auto-created by the stack) that binds
  // it to the matching Report ID in the descriptor above.
  inputKeyboard = hid.inputReport(REPORT_ID_KB);
  inputMouse = hid.inputReport(REPORT_ID_MOUSE);
  inputConsumer = hid.inputReport(REPORT_ID_CONSUMER);

  server.start();

  // -- Advertising --
  // Use HID_GENERIC appearance because this device is not exclusively a
  // keyboard or mouse -- it is a composite of several HID functions.
  BLEAdvertising adv = BLE.getAdvertising();
  adv.setAppearance(BLE_APPEARANCE_HID_GENERIC);
  adv.setScanResponse(true);
  adv.start();

  Serial.println("Advertising as composite HID. Pair from your device.");
  Serial.println("Demo will cycle: keyboard -> mouse -> media keys");
}

// ---------------------------------------------------------------------------
// loop() -- Demo cycle
//
// Rotates through the three device functions every few seconds so you can
// observe each report type on the host. In a real product you would send
// reports in response to physical buttons, sensors, or serial commands.
// ---------------------------------------------------------------------------
void loop() {
  if (!deviceConnected) {
    delay(100);
    return;
  }

  static uint32_t step = 0;
  uint32_t phase = (step / 40) % 3;  // 40 iterations per phase (~2 s each)

  switch (phase) {

    // Phase 0: Keyboard -- type "Hi" using HID key codes
    case 0:
    {
      if (step % 40 == 0) {
        Serial.println("[Keyboard] Sending 'H' (Shift + h)");
        // HID key code 0x0B = 'h'; modifier 0x02 = Left Shift held
        uint8_t keys[6] = {0x0B, 0, 0, 0, 0, 0};
        sendKeyboardReport(0x02, keys);
      } else if (step % 40 == 10) {
        releaseAllKeys();
        Serial.println("[Keyboard] Sending 'i'");
        // HID key code 0x0C = 'i'; no modifier
        uint8_t keys[6] = {0x0C, 0, 0, 0, 0, 0};
        sendKeyboardReport(0x00, keys);
      } else if (step % 40 == 20) {
        releaseAllKeys();
      }
      break;
    }

    // Phase 1: Mouse -- move in a small circle and click
    case 1:
    {
      int8_t dx = (step % 2 == 0) ? 5 : -5;
      int8_t dy = (step % 4 < 2) ? 3 : -3;
      uint8_t btn = (step % 40 < 10) ? 0x01 : 0x00;  // left-click first 10 ticks
      sendMouseReport(btn, dx, dy, 0);
      if (step % 10 == 0) {
        Serial.printf("[Mouse] dx=%d dy=%d btn=0x%02X\n", dx, dy, btn);
      }
      break;
    }

    // Phase 2: Consumer Control -- toggle media keys
    case 2:
    {
      uint32_t subStep = step % 40;
      if (subStep == 0) {
        Serial.println("[Media] Volume Up");
        sendConsumerReport(CONSUMER_VOLUME_UP);
      } else if (subStep == 8) {
        releaseConsumer();
        Serial.println("[Media] Volume Down");
        sendConsumerReport(CONSUMER_VOLUME_DOWN);
      } else if (subStep == 16) {
        releaseConsumer();
        Serial.println("[Media] Play/Pause");
        sendConsumerReport(CONSUMER_PLAY_PAUSE);
      } else if (subStep == 24) {
        releaseConsumer();
        Serial.println("[Media] Next Track");
        sendConsumerReport(CONSUMER_NEXT_TRACK);
      } else if (subStep == 32) {
        releaseConsumer();
      }
      break;
    }
  }

  step++;
  delay(50);
}
