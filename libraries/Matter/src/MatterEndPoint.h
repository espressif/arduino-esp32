#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <functional>

// Matter Endpoint Base Class. Controls the endpoint ID and allows the child class to overwrite attribute change call
class MatterEndPoint {
  public:
    uint16_t getEndPointId() {
      return endpoint_id;
    }
    void setEndPointId(uint16_t ep) {
      endpoint_id = ep;
    }
    
    virtual bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) = 0;

  protected:
    uint16_t endpoint_id = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */