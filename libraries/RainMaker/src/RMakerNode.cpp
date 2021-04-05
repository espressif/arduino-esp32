#include "RMakerNode.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_IDF_TARGET_ESP32
static esp_err_t err;

esp_err_t Node::addDevice(Device device)
{
    err = esp_rmaker_node_add_device(node, device.getDeviceHandle());
    if(err != ESP_OK){
        log_e("Device was not added to the Node");
    }   
    return err;
}

esp_err_t Node::removeDevice(Device device)
{
    err = esp_rmaker_node_remove_device(node, device.getDeviceHandle());
    if(err != ESP_OK){
        log_e("Device was not removed from the Node");
    }
    return err;
}

char *Node::getNodeID()
{
    return esp_rmaker_get_node_id();
}

node_info_t *Node::getNodeInfo()
{
    return esp_rmaker_node_get_info(node);
}

esp_err_t Node::addNodeAttr(const char *attr_name, const char *val)
{
    err = esp_rmaker_node_add_attribute(node, attr_name, val);
    if(err != ESP_OK) {
        log_e("Failed to add attribute to the Node");
    }   
    return err;
}
#endif
