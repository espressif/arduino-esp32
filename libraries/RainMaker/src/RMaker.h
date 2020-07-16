#include "Arduino.h"

#include "RMakerGeneric.h"
#include "RMakerWiFi.h"
#include "RMakerType.h"
#include "RMakerOTA.h"

#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_core.h>

class RMakerClass : public RMakerGenericClass, public RMakerWiFiClass, public RMakerOTAClass
{
};

extern RMakerClass RMaker;
