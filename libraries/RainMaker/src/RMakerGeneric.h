#include <esp_err.h>
#include "RMakerType.h"

typedef esp_err_t (*DeviceParamCb)(const char *dev_name, const char *param_name, Param_t val, void *data);

class RMakerGenericClass
{
    public:
    
        //Types

        //Rainmaker Important API
        void rmakerInit(char *node_name, char *node_type);
        esp_err_t rmakerStart();
        esp_err_t rmakerStop(); 

        //NODE INFO API
        char *getNodeID();
        esp_rmaker_node_info_t *getNodeInfo();
        esp_err_t addNodeAttr(const char *attr_name, const char *val);

        //Device Info
        esp_err_t createDevice(const char *dev_name, const char *dev_type, DeviceParamCb cb, void *data);
        esp_err_t deviceAddAttr(const char *dev_name, const char *attr_name, const char *val);
        esp_err_t deviceAddParam(const char *dev_name, const char *param_name, esp_rmaker_param_val_t val, uint8_t properties);
        esp_err_t deviceAssignPrimaryParam(const char *dev_name, const char *param_name);

        //Service Info
        esp_err_t createService(const char *serv_name, const char *type, DeviceParamCb cb, void *priv_data);
        esp_err_t serviceAddParam(const char *serv_name, const char *param_name, esp_rmaker_param_val_t val, uint8_t properties);

        //Add parameter to the device [ esp_rmaker_standard_params.h ]
        esp_err_t deviceAddNameParam(const char *dev_name, const char *param_name);
        esp_err_t deviceAddPowerParam(const char *dev_name, const char *param_name, bool val);
        esp_err_t deviceAddBrightnessParam(const char *dev_name, const char *param_name, int val);        
        esp_err_t deviceAddHueParam(const char *dev_name, const char *param_name, int val);
        esp_err_t deviceAddSaturationParam(const char *dev_name, const char *param_name, int val);
        esp_err_t deviceAddIntensityParam(const char *dev_name, const char *param_name, int val);
        esp_err_t deviceAddCctParam(const char *dev_name, const char *param_name, int val);
        esp_err_t deviceAddDirectionParam(const char *dev_name, const char *param_name, int val);
        esp_err_t deviceAddSpeedParam(const char *dev_name, const char *param_name, int val);
        esp_err_t deviceAddTempratureParam(const char *dev_name, const char *param_name, float val);
        
        //Add parameter to the service
        esp_err_t serviceAddOTAStatusParam(const char *serv_name, const char *param_name);
        esp_err_t serviceAddOTAInfoParam(const char *serv_name, const char *param_name);
        esp_err_t serviceAddOTAUrlParam(const char *serv_name, const char *param_name);

        //Parameter
        esp_err_t updateParam(const char *dev_name, const char *param_name, Param_t val);
        esp_err_t paramAddUIType(const char *dev_name, const char *name, const char *ui_type);
        esp_err_t paramAddBounds(const char *dev_name, const char *param_name, esp_rmaker_param_val_t min, esp_rmaker_param_val_t max, esp_rmaker_param_val_t step);
        esp_err_t paramAddType(const char *dev_name, const char *param_name, const char* type);
};
