#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 1
#warning This sketch should be used when USB is in OTG mode
void setup() {}
void loop() {}
#else
#include "USB.h"
#include "USBVendor.h"

USBVendor Vendor;
const int buttonPin = 0;

//CDC Control Requests
#define REQUEST_SET_LINE_CODING 0x20
#define REQUEST_GET_LINE_CODING 0x21
#define REQUEST_SET_CONTROL_LINE_STATE 0x22

//CDC Line Coding Control Request Structure
typedef struct __attribute__((packed)) {
  uint32_t bit_rate;
  uint8_t stop_bits;  //0: 1 stop bit, 1: 1.5 stop bits, 2: 2 stop bits
  uint8_t parity;     //0: None, 1: Odd, 2: Even, 3: Mark, 4: Space
  uint8_t data_bits;  //5, 6, 7, 8 or 16
} request_line_coding_t;

static request_line_coding_t vendor_line_coding = { 9600, 0, 0, 8 };

// Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
static uint8_t vendor_line_state = 0;

//USB and Vendor events
static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    arduino_usb_event_data_t* data = (arduino_usb_event_data_t*)event_data;
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT:
        Serial.println("USB PLUGGED");
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        Serial.println("USB UNPLUGGED");
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        Serial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
        break;
      case ARDUINO_USB_RESUME_EVENT:
        Serial.println("USB RESUMED");
        break;

      default:
        break;
    }
  } else if (event_base == ARDUINO_USB_VENDOR_EVENTS) {
    arduino_usb_vendor_event_data_t* data = (arduino_usb_vendor_event_data_t*)event_data;
    switch (event_id) {
      case ARDUINO_USB_VENDOR_DATA_EVENT:
        Serial.printf("Vendor RX: len:%u\n", data->data.len);
        for (uint16_t i = 0; i < data->data.len; i++) {
          Serial.write(Vendor.read());
        }
        Serial.println();
        break;

      default:
        break;
    }
  }
}

static const char* strRequestDirections[] = { "OUT", "IN" };
static const char* strRequestTypes[] = { "STANDARD", "CLASS", "VENDOR", "INVALID" };
static const char* strRequestRecipients[] = { "DEVICE", "INTERFACE", "ENDPOINT", "OTHER" };
static const char* strRequestStages[] = { "SETUP", "DATA", "ACK" };

//Handle USB requests to the vendor interface
bool vendorRequestCallback(uint8_t rhport, uint8_t requestStage, arduino_usb_control_request_t const* request) {
  Serial.printf("Vendor Request: Stage: %5s, Direction: %3s, Type: %8s, Recipient: %9s, bRequest: 0x%02x, wValue: 0x%04x, wIndex: %u, wLength: %u\n",
                strRequestStages[requestStage],
                strRequestDirections[request->bmRequestDirection],
                strRequestTypes[request->bmRequestType],
                strRequestRecipients[request->bmRequestRecipient],
                request->bRequest, request->wValue, request->wIndex, request->wLength);

  bool result = false;

  if (request->bmRequestDirection == REQUEST_DIRECTION_OUT && request->bmRequestType == REQUEST_TYPE_STANDARD && request->bmRequestRecipient == REQUEST_RECIPIENT_INTERFACE && request->bRequest == 0x0b) {
    if (requestStage == REQUEST_STAGE_SETUP) {
      // response with status OK
      result = Vendor.sendResponse(rhport, request);
    } else {
      result = true;
    }
  } else
    //Implement CDC Control Requests
    if (request->bmRequestType == REQUEST_TYPE_CLASS && request->bmRequestRecipient == REQUEST_RECIPIENT_DEVICE) {
      switch (request->bRequest) {

        case REQUEST_SET_LINE_CODING:  //0x20
          // Accept only direction OUT with data size 7
          if (request->wLength != sizeof(request_line_coding_t) || request->bmRequestDirection != REQUEST_DIRECTION_OUT) {
            break;
          }
          if (requestStage == REQUEST_STAGE_SETUP) {
            //Send the response in setup stage (it will write the data to vendor_line_coding in the DATA stage)
            result = Vendor.sendResponse(rhport, request, (void*)&vendor_line_coding, sizeof(request_line_coding_t));
          } else if (requestStage == REQUEST_STAGE_ACK) {
            //In the ACK stage the response is complete
            Serial.printf("Vendor Line Coding: bit_rate: %lu, data_bits: %u, stop_bits: %u, parity: %u\n", vendor_line_coding.bit_rate, vendor_line_coding.data_bits, vendor_line_coding.stop_bits, vendor_line_coding.parity);
          }
          result = true;
          break;

        case REQUEST_GET_LINE_CODING:  //0x21
          // Accept only direction IN with data size 7
          if (request->wLength != sizeof(request_line_coding_t) || request->bmRequestDirection != REQUEST_DIRECTION_IN) {
            break;
          }
          if (requestStage == REQUEST_STAGE_SETUP) {
            //Send the response in setup stage (it will write the data to vendor_line_coding in the DATA stage)
            result = Vendor.sendResponse(rhport, request, (void*)&vendor_line_coding, sizeof(request_line_coding_t));
          }
          result = true;
          break;

        case REQUEST_SET_CONTROL_LINE_STATE:  //0x22
          // Accept only direction OUT with data size 0
          if (request->wLength != 0 || request->bmRequestDirection != REQUEST_DIRECTION_OUT) {
            break;
          }
          if (requestStage == REQUEST_STAGE_SETUP) {
            //Send the response in setup stage
            vendor_line_state = request->wValue;
            result = Vendor.sendResponse(rhport, request);
          } else if (requestStage == REQUEST_STAGE_ACK) {
            //In the ACK stage the response is complete
            bool dtr = (vendor_line_state & 1) != 0;
            bool rts = (vendor_line_state & 2) != 0;
            Serial.printf("Vendor Line State: dtr: %u, rts: %u\n", dtr, rts);
          }
          result = true;
          break;

        default:
          // stall unknown request
          break;
      }
    }

  return result;
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Vendor.onEvent(usbEventCallback);
  Vendor.onRequest(vendorRequestCallback);
  Vendor.begin();

  USB.onEvent(usbEventCallback);
  USB.webUSB(true);
  // Set the URL for your WebUSB landing page
  USB.webUSBURL("https://docs.espressif.com/projects/arduino-esp32/en/latest/_static/webusb.html");
  USB.begin();
}

void loop() {
  static int previousButtonState = HIGH;
  int buttonState = digitalRead(buttonPin);
  if (buttonState != previousButtonState) {
    previousButtonState = buttonState;
    if (buttonState == LOW) {
      Serial.println("Button Pressed");
      Vendor.println("Button Pressed");
      Vendor.flush();  //Without flushing the data will only be sent when the buffer is full (64 bytes)
    } else {
      Serial.println("Button Released");
      Vendor.println("Button Released");
      Vendor.flush();  //Without flushing the data will only be sent when the buffer is full (64 bytes)
    }
    delay(100);
  }

  while (Serial.available()) {
    size_t l = Serial.available();
    uint8_t b[l];
    l = Serial.read(b, l);
    Vendor.write(b, l);
    Vendor.flush();  //Without flushing the data will only be sent when the buffer is full (64 bytes)
  }
}
#endif /* ARDUINO_USB_MODE */
