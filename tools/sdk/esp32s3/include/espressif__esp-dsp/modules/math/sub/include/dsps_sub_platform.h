#ifndef _dsps_sub_platform_H_
#define _dsps_sub_platform_H_

#include "sdkconfig.h"

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#define dsps_sub_f32_ae32_enabled  1
#define dsps_sub_s16_ae32_enabled  1

#endif

#if (XCHAL_HAVE_LOOPS == 1)
#define dsps_sub_f32_ae32_enabled  1
#define dsps_sub_s16_ae32_enabled  1
#endif

#if (CONFIG_IDF_TARGET_ESP32S3 == 1)
#define dsps_sub_f32_ae32_enabled  1
#define dsps_sub_s16_aes3_enabled  1
#endif

#endif // __XTENSA__

#endif // _dsps_sub_platform_H_