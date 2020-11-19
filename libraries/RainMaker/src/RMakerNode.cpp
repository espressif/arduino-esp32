/*
    RMakerNode.cpp - ESP RainMaker node support.
    Copyright (c) 2020 Arduino. All right reserved.
 
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.
  
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
  
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "RMakerNode.h"
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
