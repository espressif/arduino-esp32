#pragma once

#include <Matter.h>
#include <MatterEndPoint.h>

class MatterOnOffLight : public MatterEndPoint {
  public:
    MatterOnOffLight();
    ~MatterOnOffLight();
    virtual bool begin(bool initialState = false); // default initial state is off
    void end(); // this will just stop proessing Light Matter events

    bool setOnOff(bool newState); // returns true if successful
    bool getOnOff();              // returns current light state
    bool toggle();                // returns true if successful

    operator bool();              // returns current light state
    void operator=(bool state);   // turns light on or off
    // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
    bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);
    // User Callback for whenever the Light state is changed by the Matter Controller
    using EndPointCB = std::function<bool(bool)>;
    void onChangeOnOff(EndPointCB onChangeCB) {
      _onChangeCB = onChangeCB;
    }

  protected:
    bool started = false;
    bool state = false;   // default initial state is off, but it can be changed by begin(bool)
    EndPointCB _onChangeCB = NULL;
};