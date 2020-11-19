/*
    RMakerNode.h - ESP RainMaker node support.
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

#include "RMakerDevice.h"

class Node
{
    private:
        esp_rmaker_node_t *node;
    
    public:
        Node()
        {
            node = NULL;
        }
        void setNodeHandle(esp_rmaker_node_t *rnode)
        {
            node = rnode;
        }
        esp_rmaker_node_t *getNodeHandle()
        {
            return node;
        }
    
        esp_err_t addDevice(Device device);
        esp_err_t removeDevice(Device device);

        char *getNodeID();
        node_info_t *getNodeInfo();
        esp_err_t addNodeAttr(const char *attr_name, const char *val);
};
