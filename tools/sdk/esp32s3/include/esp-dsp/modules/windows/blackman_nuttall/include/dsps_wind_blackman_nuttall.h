// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
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


#ifndef _dsps_wind_blackman_nuttall_H_
#define _dsps_wind_blackman_nuttall_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   Blackman-Nuttall window
 * 
 * The function generates Blackman-Nuttall window.
 *
 * @param window: buffer to store window array.
 * @param len: length of the window array
 *
 */
void dsps_wind_blackman_nuttall_f32(float *window, int len);

#ifdef __cplusplus
}
#endif
#endif // _dsps_wind_blackman_nuttall_H_