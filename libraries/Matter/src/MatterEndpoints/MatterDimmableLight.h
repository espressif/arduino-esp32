#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>

class MatterDimmableLight : public MatterEndPoint {
public:
  static const uint8_t MAX_BRIGHTNESS = 255;

  MatterDimmableLight();
  ~MatterDimmableLight();
  // default initial state is off and brightness is 0
  virtual bool begin(bool initialState = false, uint8_t brightness = 0);
  // this will just stop processing Light Matter events
  void end();

  bool setOnOff(bool newState);  // returns true if successful
  bool getOnOff();               // returns current light state
  bool toggle();                 // returns true if successful

  bool setBrightness(uint8_t newBrightness);  // returns true if successful
  uint8_t getBrightness();                    // returns current brightness

  operator bool();             // returns current on/off light state
  void operator=(bool state);  // turns light on or off
  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);
  // User Callback for whenever the Light On/Off state is changed by the Matter Controller
  using EndPointOnOffCB = std::function<bool(bool)>;
  void onChangeOnOff(EndPointOnOffCB onChangeCB) {
    _onChangeOnOffCB = onChangeCB;
  }
  // User Callback for whenever the Light brightness value [0..255] is changed by the Matter Controller
  using EndPointBrightnessCB = std::function<bool(uint8_t)>;
  void onChangeBrightness(EndPointBrightnessCB onChangeCB) {
    _onChangeBrightnessCB = onChangeCB;
  }

protected:
  bool started = false;
  bool onOffState = false;      // default initial state is off, but it can be changed by begin(bool)
  uint8_t brightnessLevel = 0;  // default initial brightness is 0, but it can be changed by begin(bool, uint8_t)
  EndPointOnOffCB _onChangeOnOffCB = NULL;
  EndPointBrightnessCB _onChangeBrightnessCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
