#include "USB.h"
#include "USBHID.h"
USBHID HID;

static const uint8_t report_descriptor[] = { // 8 axis
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xa1, 0x01,        // Collection (Application)
    0xa1, 0x00,        //   Collection (Physical)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x33,        //     Usage (Rx)
    0x09, 0x34,        //     Usage (Ry)
    0x09, 0x35,        //     Usage (Rz)
    0x09, 0x36,        //     Usage (Slider)
    0x09, 0x36,        //     Usage (Slider)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7f,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x08,        //     Report Count (8)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

class CustomHIDDevice: public USBHIDDevice {
public:
  CustomHIDDevice(void){
    static bool initialized = false;
    if(!initialized){
      initialized = true;
      HID.addDevice(this, sizeof(report_descriptor));
    }
  }
  
  void begin(void){
    HID.begin();
  }
    
  uint16_t _onGetDescriptor(uint8_t* buffer){
    memcpy(buffer, report_descriptor, sizeof(report_descriptor));
    return sizeof(report_descriptor);
  }

  bool send(uint8_t * value){
    return HID.SendReport(0, value, 8);
  }
};

CustomHIDDevice Device;

const int buttonPin = 0;
int previousButtonState = HIGH;
uint8_t axis[8];

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(buttonPin, INPUT_PULLUP);
  Device.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if (HID.ready() && buttonState != previousButtonState) {
    previousButtonState = buttonState;
    if (buttonState == LOW) {
      Serial.println("Button Pressed");
      axis[0] = random() & 0xFF;
      Device.send(axis);
    } else {
      Serial.println("Button Released");
    }
    delay(100);
  }
}
