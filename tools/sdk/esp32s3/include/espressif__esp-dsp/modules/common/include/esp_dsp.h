// Copyright 2018-2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _esp_dsp_H_
#define _esp_dsp_H_

#ifdef __cplusplus
extern "C"
{
#endif

// Common includes
#include "dsp_common.h"
#include "dsp_types.h"

// Signal processing
#include "dsps_dotprod.h"
#include "dsps_math.h"
#include "dsps_fir.h"
#include "dsps_biquad.h"
#include "dsps_biquad_gen.h"
#include "dsps_wind.h"
#include "dsps_conv.h"
#include "dsps_corr.h"

#include "dsps_d_gen.h"
#include "dsps_h_gen.h"
#include "dsps_tone_gen.h"
#include "dsps_snr.h"
#include "dsps_sfdr.h"

#include "dsps_fft2r.h"
#include "dsps_fft4r.h"
#include "dsps_dct.h"

// Matrix operations
#include "dspm_matrix.h"

// Support functions
#include "dsps_view.h"

// Image processing functions:
#include "dspi_dotprod.h"


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "mat.h"
#endif

#endif // _esp_dsp_H_