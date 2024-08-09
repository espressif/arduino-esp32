/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeSwitch : public Zigbee_EP {
  public:
    ZigbeeSwitch(uint8_t endpoint);
    ~ZigbeeSwitch();

    // methods to control the on/off light
    void lightToggle();
    void lightOn();
    void lightOff();
    void lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant);
    void lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off);
    void lightOnWithSceneRecall();

  private:
    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
    static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
    static void find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

    static uint8_t get_endpoint() { return _endpoint; }

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
