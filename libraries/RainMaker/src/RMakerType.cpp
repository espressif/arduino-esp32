#include "RMakerType.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_ESP_RMAKER_TASK_STACK && CONFIG_IDF_TARGET_ESP32

param_val_t value(int ival)
{
    return esp_rmaker_int(ival);
}

param_val_t value(bool bval)
{
    return esp_rmaker_bool(bval);
}

param_val_t value(char *sval)
{
    return esp_rmaker_str(sval);
}

param_val_t value(float fval)
{
    return esp_rmaker_float(fval);
}

#endif