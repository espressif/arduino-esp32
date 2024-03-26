#ifndef _dsps_fft2r_platform_H_
#define _dsps_fft2r_platform_H_

#include "sdkconfig.h"

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#define dsps_fft2r_fc32_ae32_enabled 1

#endif //

#if ((XCHAL_HAVE_LOOPS == 1) && (XCHAL_HAVE_MAC16 == 1))

#define dsps_fft2r_sc16_ae32_enabled 1

#endif //

#if (XCHAL_HAVE_LOOPS == 1)

#define dsps_bit_rev_lookup_fc32_ae32_enabled 1

#endif //
#endif // __XTENSA__

#if CONFIG_IDF_TARGET_ESP32S3
#define dsps_fft2r_fc32_aes3_enabled 1
#define dsps_fft2r_sc16_aes3_enabled 1
#endif


#endif // _dsps_fft2r_platform_H_
