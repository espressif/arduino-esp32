#ifndef _dsps_conv_platform_H_
#define _dsps_conv_platform_H_

#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#define dsps_conv_f32_ae32_enabled  1
#define dsps_ccorr_f32_ae32_enabled  1
#define dsps_corr_f32_ae32_enabled  1

#endif

#endif // _dsps_conv_platform_H_