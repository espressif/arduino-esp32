#ifndef _dsps_mulc_platform_H_
#define _dsps_mulc_platform_H_

#include "sdkconfig.h"

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#include <xtensa/config/core-matmap.h>


#if ((XCHAL_HAVE_FP == 1) && (XCHAL_HAVE_LOOPS == 1))

#define dsps_mulc_f32_ae32_enabled  1

#endif

#if ((XCHAL_HAVE_LOOPS == 1) && (XCHAL_HAVE_MAC16 == 1))

#define dsps_mulc_s16_ae32_enabled 1

#endif //
#endif // __XTENSA__


#endif // _dsps_mulc_platform_H_