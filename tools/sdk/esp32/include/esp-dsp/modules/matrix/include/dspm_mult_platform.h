#ifndef _dspm_mult_platform_H_
#define _dspm_mult_platform_H_



#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#define dspm_mult_f32_ae32_enabled 1
#define dspm_mult_3x3x1_f32_ae32_enabled 1
#define dspm_mult_3x3x3_f32_ae32_enabled 1
#define dspm_mult_4x4x1_f32_ae32_enabled 1
#define dspm_mult_4x4x4_f32_ae32_enabled 1

#endif

#if ((XCHAL_HAVE_LOOPS == 1) && (XCHAL_HAVE_MAC16 == 1))

#define dspm_mult_s16_ae32_enabled 1

#endif

#endif // _dspm_mult_platform_H_