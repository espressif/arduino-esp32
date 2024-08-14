/* Common Class for Zigbee End Device */

#pragma once

#include "Zigbee_core.h"
#include <Arduino.h>

/* Usefull defines */
#define ZB_ARRAY_LENTH(arr) (sizeof(arr) / sizeof(arr[0]))
#define print_ieee_addr(addr) log_i("IEEE Address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7])
#define XYZ_to_RGB(X, Y, Z, r, g, b)                        \
{                                                           \
  r = (float)( 3.240479*(X) -1.537150*(Y) -0.498535*(Z));   \
  g = (float)(-0.969256*(X) +1.875992*(Y) +0.041556*(Z));   \
  b = (float)( 0.055648*(X) -0.204043*(Y) +1.057311*(Z));   \
  if(r>1){r=1;}                                             \
  if(g>1){g=1;}                                             \
  if(b>1){b=1;}                                             \
}

#define RGB_to_XYZ(r, g, b, X, Y, Z)                          \
{                                                             \
    X = (float)(0.412453*(r) + 0.357580*(g) + 0.180423*(b));  \
    Y = (float)(0.212671*(r) + 0.715160*(g) + 0.072169*(b));  \
    Z = (float)(0.019334*(r) + 0.119193*(g) + 0.950227*(b));  \
}

typedef struct zbstring_s {
  uint8_t len;
  char data[];
} ESP_ZB_PACKED_STRUCT zbstring_t;

/* Zigbee End Device Class */
class Zigbee_EP {
  public:
    Zigbee_EP(uint8_t endpoint = 10);
    ~Zigbee_EP();

    // Find endpoint may be implemented by EPs
    virtual void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {};

    static uint8_t _endpoint;
    esp_zb_ha_standard_devices_t _device_id; //type of device
    uint8_t _version;

    esp_zb_endpoint_config_t _ep_config;
    esp_zb_cluster_list_t *_cluster_list;

    // Set ep config and cluster list
    void set_ep_config(esp_zb_endpoint_config_t ep_config, esp_zb_cluster_list_t *cluster_list) {
        _ep_config = ep_config;
        _cluster_list = cluster_list;
    }

    static bool _is_bound;
    static bool is_bound() { return _is_bound; }

    static bool _allow_multiple_binding;
    static void allowMultipleBinding(bool bind) { _allow_multiple_binding = bind; }

    // Manufacturer name and model implemented
    void setManufacturerAndModel(const char *name, const char *model);
    void readManufacturerAndModel(uint8_t endpoint, uint16_t short_addr);

    virtual void readManufacturer(char* manufacturer) {};
    virtual void readModel(char* model) {};

    //list of all handlers function calls, to be overide by EPs implementation
    virtual void attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) {};
    virtual void attribute_read_cmd(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute); //already implemented

    friend class ZigbeeCore;

    // List of Zigbee EP classes 
    friend class ZigbeeLight;

    /* TODO: create all possible methods for each EP type, that can be overwritten by user to use them
    
  // NOTE: This is a list of all possible callbacks that can be used in Zigbee_EP class
  // typedef enum esp_zb_core_action_callback_id_s {
  //     ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID                    = 0x0000,   /*!< Set attribute value, refer to esp_zb_zcl_set_attr_value_message_t */
  //     ESP_ZB_CORE_SCENES_STORE_SCENE_CB_ID                = 0x0001,   /*!< Store scene, refer to esp_zb_zcl_store_scene_message_t */
  //     ESP_ZB_CORE_SCENES_RECALL_SCENE_CB_ID               = 0x0002,   /*!< Recall scene, refer to esp_zb_zcl_recall_scene_message_t */
  //     ESP_ZB_CORE_IAS_ZONE_ENROLL_RESPONSE_VALUE_CB_ID    = 0x0003,   /*!< IAS Zone enroll response, refer to esp_zb_zcl_ias_zone_enroll_response_message_t */
  //     ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID                 = 0x0004,   /*!< Upgrade OTA, refer to esp_zb_zcl_ota_upgrade_value_message_t */
  //     ESP_ZB_CORE_OTA_UPGRADE_SRV_STATUS_CB_ID            = 0x0005,   /*!< OTA Server status, refer to esp_zb_zcl_ota_upgrade_server_status_message_t */
  //     ESP_ZB_CORE_OTA_UPGRADE_SRV_QUERY_IMAGE_CB_ID       = 0x0006,   /*!< OTA Server query image, refer to esp_zb_zcl_ota_upgrade_server_query_image_message_t */
  //     ESP_ZB_CORE_THERMOSTAT_VALUE_CB_ID                  = 0x0007,   /*!< Thermostat value, refer to esp_zb_zcl_thermostat_value_message_t */
  //     ESP_ZB_CORE_METERING_GET_PROFILE_CB_ID              = 0x0008,   /*!< Metering get profile, refer to esp_zb_zcl_metering_get_profile_message_t */
  //     ESP_ZB_CORE_METERING_GET_PROFILE_RESP_CB_ID         = 0x0009,   /*!< Metering get profile response, refer to esp_zb_zcl_metering_get_profile_resp_message_t */
  //     ESP_ZB_CORE_METERING_REQ_FAST_POLL_MODE_CB_ID       = 0x000a,   /*!< Metering request fast poll mode, refer to esp_zb_zcl_metering_request_fast_poll_mode_message_t */
  //     ESP_ZB_CORE_METERING_REQ_FAST_POLL_MODE_RESP_CB_ID  = 0x000b,   /*!< Metering request fast poll mode response, refer to esp_zb_zcl_metering_request_fast_poll_mode_resp_message_t */
  //     ESP_ZB_CORE_METERING_GET_SNAPSHOT_CB_ID             = 0x000c,   /*!< Metering get snapshot, refer to esp_zb_zcl_metering_get_snapshot_message_t */
  //     ESP_ZB_CORE_METERING_PUBLISH_SNAPSHOT_CB_ID         = 0x000d,   /*!< Metering publish snapshot, refer to esp_zb_zcl_metering_publish_snapshot_message_t */
  //     ESP_ZB_CORE_METERING_GET_SAMPLED_DATA_CB_ID         = 0x000e,   /*!< Metering get sampled data, refer to esp_zb_zcl_metering_get_sampled_data_message_t */
  //     ESP_ZB_CORE_METERING_GET_SAMPLED_DATA_RESP_CB_ID    = 0x000f,   /*!< Metering get sampled data response, refer to esp_zb_zcl_metering_get_sampled_data_resp_message_t */
  //     ESP_ZB_CORE_DOOR_LOCK_LOCK_DOOR_CB_ID               = 0x0010,   /*!< Lock/unlock door request, refer to esp_zb_zcl_door_lock_lock_door_message_t */
  //     ESP_ZB_CORE_DOOR_LOCK_LOCK_DOOR_RESP_CB_ID          = 0x0011,   /*!< Lock/unlock door response, refer to esp_zb_zcl_door_lock_lock_door_resp_message_t */
  //     ESP_ZB_CORE_IDENTIFY_EFFECT_CB_ID                   = 0x0012,   /*!< Identify triggers effect request, refer to esp_zb_zcl_identify_effect_message_t */
  //     ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID                = 0x1000,   /*!< Read attribute response, refer to esp_zb_zcl_cmd_read_attr_resp_message_t */
  //     ESP_ZB_CORE_CMD_WRITE_ATTR_RESP_CB_ID               = 0x1001,   /*!< Write attribute response, refer to esp_zb_zcl_cmd_write_attr_resp_message_t */
  //     ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID            = 0x1002,   /*!< Configure reprot response, refer to esp_zb_zcl_cmd_config_report_resp_message_t */
  //     ESP_ZB_CORE_CMD_READ_REPORT_CFG_RESP_CB_ID          = 0x1003,   /*!< Read report configuration response, refer to esp_zb_zcl_cmd_read_report_config_resp_message_t */
  //     ESP_ZB_CORE_CMD_DISC_ATTR_RESP_CB_ID                = 0x1004,   /*!< Discover attributes response, refer to esp_zb_zcl_cmd_discover_attributes_resp_message_t */
  //     ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID                  = 0x1005,   /*!< Default response, refer to esp_zb_zcl_cmd_default_resp_message_t */
  //     ESP_ZB_CORE_CMD_OPERATE_GROUP_RESP_CB_ID            = 0x1010,   /*!< Group add group response, refer to esp_zb_zcl_groups_operate_group_resp_message_t */
  //     ESP_ZB_CORE_CMD_VIEW_GROUP_RESP_CB_ID               = 0x1011,   /*!< Group view response, refer to esp_zb_zcl_groups_view_group_resp_message_t */
  //     ESP_ZB_CORE_CMD_GET_GROUP_MEMBERSHIP_RESP_CB_ID     = 0x1012,   /*!< Group get membership response, refer to esp_zb_zcl_groups_get_group_membership_resp_message_t */
  //     ESP_ZB_CORE_CMD_OPERATE_SCENE_RESP_CB_ID            = 0x1020,   /*!< Scenes operate response, refer to esp_zb_zcl_scenes_operate_scene_resp_message_t */
  //     ESP_ZB_CORE_CMD_VIEW_SCENE_RESP_CB_ID               = 0x1021,   /*!< Scenes view response, refer to esp_zb_zcl_scenes_view_scene_resp_message_t */
  //     ESP_ZB_CORE_CMD_GET_SCENE_MEMBERSHIP_RESP_CB_ID     = 0x1022,   /*!< Scenes get membership response, refer to esp_zb_zcl_scenes_get_scene_membership_resp_message_t */
  //     ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_ENROLL_REQUEST_ID     = 0x1030,   /*!< IAS Zone enroll request, refer to esp_zb_zcl_ias_zone_enroll_request_message_t */
  //     ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_STATUS_CHANGE_NOT_ID  = 0x1031,   /*!< IAS Zone status change notification, refer to esp_zb_zcl_ias_zone_status_change_notification_message_t */
  //     ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID            = 0x1040,   /*!< Custom Cluster request, refer to esp_zb_zcl_custom_cluster_command_message_t */
  //     ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_RESP_CB_ID           = 0x1041,   /*!< Custom Cluster response, refer to esp_zb_zcl_custom_cluster_command_message_t */
  //     ESP_ZB_CORE_CMD_PRIVILEGE_COMMAND_REQ_CB_ID         = 0x1050,   /*!< Custom Cluster request, refer to esp_zb_zcl_privilege_command_message_t */
  //     ESP_ZB_CORE_CMD_PRIVILEGE_COMMAND_RESP_CB_ID        = 0x1051,   /*!< Custom Cluster response, refer to esp_zb_zcl_privilege_command_message_t */
  //     ESP_ZB_CORE_CMD_GREEN_POWER_RECV_CB_ID              = 0x1F00,   /*!< Green power cluster command receiving, refer to esp_zb_zcl_cmd_green_power_recv_message_t */
  //     ESP_ZB_CORE_REPORT_ATTR_CB_ID                       = 0x2000,   /*!< Attribute Report, refer to esp_zb_zcl_report_attr_message_t */
  // } esp_zb_core_action_callback_id_t;
  

};

