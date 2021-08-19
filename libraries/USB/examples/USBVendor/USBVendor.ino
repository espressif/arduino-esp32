#include "USB.h"
#include "USBVendor.h"
#include "class/cdc/cdc.h"

#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#else
#define HWSerial Serial
#endif

USBVendor Vendor;

const int buttonPin = 0;
int previousButtonState = HIGH;

static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if(event_base == ARDUINO_USB_EVENTS){
    arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_STARTED_EVENT:
        HWSerial.println("USB PLUGGED");
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        HWSerial.println("USB UNPLUGGED");
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        HWSerial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
        break;
      case ARDUINO_USB_RESUME_EVENT:
        HWSerial.println("USB RESUMED");
        break;
      
      default:
        break;
    }
  } else if(event_base == ARDUINO_USB_VENDOR_EVENTS){
    arduino_usb_vendor_event_data_t * data = (arduino_usb_vendor_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_VENDOR_DATA_EVENT:
        HWSerial.printf("Vendor RX: len:%u\n", data->data.len);
        for(uint16_t i=0; i<data->data.len; i++){
          HWSerial.write(Vendor.read());
        }
        HWSerial.println();
        break;
        
      default:
        break;
    }
  }
}

bool vendorRequestCallback(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request){
    static const char * vendorRequestTypes[] = {"Standard", "Class", "Vendor"};
    static const char * vendorRequestRecipients[] = {"Device", "Interface", "Endpoint", "Other"};
    static const char * vendorStages[] = {"Setup","Data","Ack"};
    HWSerial.printf("Vendor Request: Stage: %5s, Direction: %3s, Type: %8s, Recipient: %9s, bRequest: 0x%02x, wValue: 0x%04x, wIndex: %u, wLength: %u\n", vendorStages[stage], 
        request->bmRequestType_bit.direction?"IN":"OUT", 
        vendorRequestTypes[request->bmRequestType_bit.type], 
        vendorRequestRecipients[request->bmRequestType_bit.recipient], 
        request->bRequest, request->wValue, request->wIndex, request->wLength);
    
    
    static uint8_t vendor_line_state = 0;// Bit 0:  DTR (Data Terminal Ready), Bit 1: RTS (Request to Send)
    static cdc_line_coding_t vendor_line_coding;
    bool result = false;

    if(request->bmRequestType_bit.direction == TUSB_DIR_OUT && request->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD && request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE && request->bRequest == 0x0b){
        if(stage == CONTROL_STAGE_SETUP) {
            // response with status OK
            result = Vendor.sendResponse(rhport, request);
        } else {
            result = true;
        }
    } else if(request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS && request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_DEVICE){
        //Implement CDC Control Requests
        switch (request->bRequest) {

            // CDC Set Line Coding
            case CDC_REQUEST_SET_LINE_CODING: //0x20
                if(request->wLength != sizeof(cdc_line_coding_t) || request->bmRequestType_bit.direction != TUSB_DIR_OUT){
                    break;
                }
                if(stage == CONTROL_STAGE_SETUP) {
                    //Send the response in setup stage (it will write the data to vendor_line_coding in the DATA stage)
                    result = Vendor.sendResponse(rhport, request, (void*) &vendor_line_coding, sizeof(cdc_line_coding_t));
                } else if(stage == CONTROL_STAGE_ACK){
                    //In the ACK stage the requst->response is complete
                    HWSerial.printf("Vendor Line Coding: bit_rate: %u, data_bits: %u, stop_bits: %u, parity: %u\n", vendor_line_coding.bit_rate, vendor_line_coding.data_bits, vendor_line_coding.stop_bits, vendor_line_coding.parity);
                }
                result = true;
            break;

            // CDC Get Line Coding
            case CDC_REQUEST_GET_LINE_CODING: //0x21
                if(request->wLength != sizeof(cdc_line_coding_t) || request->bmRequestType_bit.direction != TUSB_DIR_IN){
                    break;
                }
                if(stage == CONTROL_STAGE_SETUP) {
                    result = Vendor.sendResponse(rhport, request, (void*) &vendor_line_coding, sizeof(cdc_line_coding_t));
                }
                result = true;
            break;

            // CDC Set Line State
            case CDC_REQUEST_SET_CONTROL_LINE_STATE: //0x22
                if(request->wLength != 0 || request->bmRequestType_bit.direction != TUSB_DIR_OUT){
                    break;
                }
                if(stage == CONTROL_STAGE_SETUP) {
                    vendor_line_state = request->wValue;
                    result = Vendor.sendResponse(rhport, request);
                } else if(stage == CONTROL_STAGE_ACK){
                    bool dtr = tu_bit_test(vendor_line_state, 0);
                    bool rts = tu_bit_test(vendor_line_state, 1);
                    HWSerial.printf("Vendor Line State: dtr: %u, rts: %u\n", dtr, rts);
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
  HWSerial.begin(115200);
  HWSerial.setDebugOutput(true);
  
  Vendor.onEvent(usbEventCallback);
  Vendor.onRequest(vendorRequestCallback);
  Vendor.begin();
  
  USB.onEvent(usbEventCallback);
  USB.webUSB(true);
  USB.webUSBURL("http://localhost/webusb");
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if (buttonState != previousButtonState) {
    previousButtonState = buttonState;
    if (buttonState == LOW) {
      HWSerial.println("Button Pressed");
      Vendor.println("Button Pressed");
    } else {
      Vendor.println("Button Released");
      HWSerial.println("Button Released");
    }
    delay(100);
  }
  
  while(HWSerial.available()){
    size_t l = HWSerial.available();
    uint8_t b[l];
    l = HWSerial.read(b, l);
    Vendor.write(b,l);
  }
}
