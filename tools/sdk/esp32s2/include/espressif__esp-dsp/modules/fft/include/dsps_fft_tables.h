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

#ifndef _dsps_fft_tables_H_
#define _dsps_fft_tables_H_


#ifdef __cplusplus
extern "C"
{
#endif
extern const uint16_t bitrev2r_table_16_fc32[];
extern const uint16_t bitrev2r_table_16_fc32_size;

extern const uint16_t bitrev2r_table_32_fc32[];
extern const uint16_t bitrev2r_table_32_fc32_size;

extern const uint16_t bitrev2r_table_64_fc32[];
extern const uint16_t bitrev2r_table_64_fc32_size;

extern const uint16_t bitrev2r_table_128_fc32[];
extern const uint16_t bitrev2r_table_128_fc32_size;

extern const uint16_t bitrev2r_table_256_fc32[];
extern const uint16_t bitrev2r_table_256_fc32_size;

extern const uint16_t bitrev2r_table_512_fc32[];
extern const uint16_t bitrev2r_table_512_fc32_size;

extern const uint16_t bitrev2r_table_1024_fc32[];
extern const uint16_t bitrev2r_table_1024_fc32_size;

extern const uint16_t bitrev2r_table_2048_fc32[];
extern const uint16_t bitrev2r_table_2048_fc32_size;

extern const uint16_t bitrev2r_table_4096_fc32[];
extern const uint16_t bitrev2r_table_4096_fc32_size;

void dsps_fft2r_rev_tables_init_fc32(void);
extern uint16_t *dsps_fft2r_rev_tables_fc32[];
extern const uint16_t dsps_fft2r_rev_tables_fc32_size[];

extern const uint16_t bitrev4r_table_16_fc32[];
extern const uint16_t bitrev4r_table_16_fc32_size;

extern const uint16_t bitrev4r_table_32_fc32[];
extern const uint16_t bitrev4r_table_32_fc32_size;

extern const uint16_t bitrev4r_table_64_fc32[];
extern const uint16_t bitrev4r_table_64_fc32_size;

extern const uint16_t bitrev4r_table_128_fc32[];
extern const uint16_t bitrev4r_table_128_fc32_size;

extern const uint16_t bitrev4r_table_256_fc32[];
extern const uint16_t bitrev4r_table_256_fc32_size;

extern const uint16_t bitrev4r_table_512_fc32[];
extern const uint16_t bitrev4r_table_512_fc32_size;

extern const uint16_t bitrev4r_table_1024_fc32[];
extern const uint16_t bitrev4r_table_1024_fc32_size;

extern const uint16_t bitrev4r_table_2048_fc32[];
extern const uint16_t bitrev4r_table_2048_fc32_size;

extern const uint16_t bitrev4r_table_4096_fc32[];
extern const uint16_t bitrev4r_table_4096_fc32_size;

void dsps_fft4r_rev_tables_init_fc32(void);
extern uint16_t *dsps_fft4r_rev_tables_fc32[];
extern const uint16_t dsps_fft4r_rev_tables_fc32_size[];

#ifdef __cplusplus
}
#endif

#endif // _dsps_fft_tables_H_