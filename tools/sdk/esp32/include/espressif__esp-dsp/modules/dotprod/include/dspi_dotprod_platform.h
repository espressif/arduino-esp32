#ifndef _dspi_dotprod_platform_H_
#define _dspi_dotprod_platform_H_

#include "sdkconfig.h"

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if CONFIG_IDF_TARGET_ESP32S3
#define dspi_dotprod_aes3_enabled 1
#endif 
#endif // __XTENSA__

#endif // _dspi_dotprod_platform_H_
