#ifndef _dsps_mem_platform_H_
#define _dsps_mem_platform_H_

#include "sdkconfig.h"

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#if CONFIG_IDF_TARGET_ESP32S3
    #define dsps_mem_aes3_enbled  1
#else
    #define dsps_mem_aes3_enbled  0
#endif // CONFIG_IDF_TARGET_ESP32S3

#endif // 
#endif // __XTENSA__
#endif // _dsps_mem_platform_H_
