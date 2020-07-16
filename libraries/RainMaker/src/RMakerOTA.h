#include <esp_err.h>
#include "RMakerType.h"

class RMakerOTAClass
{
    public:
        esp_err_t otaEnable(OTAType_t type);
};
