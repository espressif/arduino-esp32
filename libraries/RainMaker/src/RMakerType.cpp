#include "RMakerType.h"

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
