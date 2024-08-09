/* Zigbee Common Functions */
#include "Zigbee_core.h"
#include "Arduino.h"

// forward declaration of all implemented handlers
static esp_err_t zb_attribute_set_handler(const esp_zb_zcl_set_attr_value_message_t *message);
static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message);

// Zigbee action handlers
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  /* TODO: 
    Implement handlers for different Zigbee actions (callback_id's)
  */
  // NOTE: Implement all Zigbee actions that can be handled by the Zigbee_Core class, or just call user defined callback function and let the user handle the action and read the message properly
  // NOTE: This may me harder for users, to know what callback_id's are available and what message type is received
  switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:          ret = zb_attribute_set_handler((esp_zb_zcl_set_attr_value_message_t *)message); break;
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:             ret = zb_attribute_reporting_handler((esp_zb_zcl_report_attr_message_t *)message); break;
    // case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:      ret = zb_read_attr_resp_handler((esp_zb_zcl_cmd_read_attr_resp_message_t *)message); break;
    // case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:  ret = zb_configure_report_resp_handler((esp_zb_zcl_cmd_config_report_resp_message_t *)message); break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:        log_i("Received default response"); break;
    default:                                 log_w("Receive unhandled Zigbee action(0x%x) callback", callback_id); break;
  }
  return ret;
}

static esp_err_t zb_attribute_set_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (!message) {
    log_e("Empty message");
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
  }

  log_i(
    "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster, message->attribute.id,
    message->attribute.data.size
  );

  // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<Zigbee_EP*>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->info.dst_endpoint == (*it)->_endpoint) {
      //TODO: implement argument passing to the callback function
      //if Zigbee_EP argument is set, pass it to the callback function
      // if ((*it)->_arg) {
      //   (*it)->_cb(message, (*it)->_arg);
      // }
      // else {
      (*it)->attribute_set(message); //method zb_attribute_set_handler in the LIGHT EP
      // }
    }
  }
  return ESP_OK;
}

static esp_err_t zb_attribute_reporting_handler(const esp_zb_zcl_report_attr_message_t *message) {
  if (!message) {
    log_e("Empty message");
  }
  if (message->status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->status);
  }
  log_i(
    "Received report from address(0x%x) src endpoint(%d) to dst endpoint(%d) cluster(0x%x)", message->src_address.u.short_addr, message->src_endpoint,
    message->dst_endpoint, message->cluster
  );
    // List through all Zigbee EPs and call the callback function, with the message
  for (std::list<Zigbee_EP*>::iterator it = Zigbee.ep_objects.begin(); it != Zigbee.ep_objects.end(); ++it) {
    if (message->dst_endpoint == (*it)->_endpoint) {
      //TODO: implement argument passing to the callback function
      //if Zigbee_EP argument is set, pass it to the callback function
      // if ((*it)->_arg) {
      //   (*it)->_cb(message, (*it)->_arg);
      // }
      // else {
      //(*it)->_cb(message); //method zb_attribute_reporting_handler in the EP
      // }
    }
  }
  return ESP_OK;
}
