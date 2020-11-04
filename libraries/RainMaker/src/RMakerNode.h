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
