/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

typedef struct light_bulb_device_params_s {
    esp_zb_ieee_addr_t ieee_addr;
    uint8_t  endpoint;
    uint16_t short_addr;
} light_bulb_device_params_t;

class ZigbeeColorDimmerSwitch : public Zigbee_EP {
  public:
    ZigbeeColorDimmerSwitch(uint8_t endpoint);
    ~ZigbeeColorDimmerSwitch();

    // save instance of the class in order to use it in static functions
    static ZigbeeColorDimmerSwitch* _instance;

    // list of bounded lights
    std::list<light_bulb_device_params_t*> _bound_lights;
    void printBoundLights();

    void calculateXY(uint8_t red, uint8_t green, uint8_t blue, uint16_t &x, uint16_t &y);

    // methods to control the color dimmable light
    void lightToggle();
    void lightToggle(uint16_t group_addr);
    void lightToggle(uint8_t endpoint, uint16_t short_addr);

    void lightOn();
    void lightOn(uint16_t group_addr);
    void lightOn(uint8_t endpoint, uint16_t short_addr);

    void lightOff();
    void lightOff(uint16_t group_addr);
    void lightOff(uint8_t endpoint, uint16_t short_addr);

    void lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant);
    void lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off);
    void lightOnWithSceneRecall();

    void setLightLevel(uint8_t level);
    void setLightLevel(uint8_t level, uint16_t group_addr);
    void setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr);

    void setLightColor(uint8_t red, uint8_t green, uint8_t blue);
    void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr);
    void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr);

    void setLightColorSaturation(uint8_t value);
    void setLightColorSaturation(uint8_t value, uint16_t group_addr);
    void setLightColorSaturation(uint8_t value, uint8_t endpoint, uint16_t short_addr);

    void setLightColorHue(uint8_t value);
    void setLightColorHue(uint8_t value, uint16_t group_addr);
    void setLightColorHue(uint8_t value, uint8_t endpoint, uint16_t short_addr);

  private:
    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
    static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
    static void find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

};

//NOTE:
/* ON/OFF switch commands for light control */
// typedef enum {
//     ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID                         = 0x00, /*!< "Turn off" command. */
//     ESP_ZB_ZCL_CMD_ON_OFF_ON_ID                          = 0x01, /*!< "Turn on" command. */
//     ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID                      = 0x02, /*!< "Toggle state" command. */
//     ESP_ZB_ZCL_CMD_ON_OFF_OFF_WITH_EFFECT_ID             = 0x40, /*!< "Off with effect" command. */
//     ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_RECALL_GLOBAL_SCENE_ID = 0x41, /*!< "On with recall global scene" command. */
//     ESP_ZB_ZCL_CMD_ON_OFF_ON_WITH_TIMED_OFF_ID           = 0x42, /*!< "On with timed off" command. */
// } esp_zb_zcl_on_off_cmd_id_t;
