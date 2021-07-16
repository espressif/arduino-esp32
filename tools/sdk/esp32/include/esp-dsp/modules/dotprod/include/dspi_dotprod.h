
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


#ifndef _dspi_dotprod_H_
#define _dspi_dotprod_H_

#include "esp_log.h"
#include "dsp_err.h"
#include "dsp_types.h"
#include "dspi_dotprod_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**@{*/
/**
 * @brief      dot product of two images
 * Dot product calculation for two floating point images: *out_value += image[i*...] * src2[i*...]); i= [0..count_x*count_y)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform. 
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] in_image  descriptor of the image
 * @param[in] filter  descriptor of the filter
 * @param[out] out_value   pointer to the output value
 * @param[in] count_x amount of samples by X axis  (count_x*step_X <= widdth)
 * @param[in] count_y amount of samples by Y axis (count_y*step_Y  <= height)
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspi_dotprod_f32_ansi(image2d_t* in_image, image2d_t* filter, float *out_value, int count_x, int count_y);
/**@}*/ 

/**@{*/
/**
 * @brief      dot product of two images
 * Dot product calculation for two floating point images: *out_value += image[i*...] * src2[i*...]); i= [0..count_x*count_y)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform. 
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] in_image  descriptor of the image
 * @param[in] filter  descriptor of the filter
 * @param[out] out_value   pointer to the output value
 * @param[in] count_x amount of samples by X axis  (count_x*step_X <= widdth)
 * @param[in] count_y amount of samples by Y axis (count_y*step_Y  <= height)
 * @param[in] shift - result shift to right, by default must be 15 for int16_t or 7 for int8_t 
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspi_dotprod_s16_ansi(image2d_t* in_image, image2d_t* filter, int16_t *out_value, int count_x, int count_y, int shift);
esp_err_t dspi_dotprod_u16_ansi(image2d_t* in_image, image2d_t* filter, uint16_t *out_value, int count_x, int count_y, int shift);
esp_err_t dspi_dotprod_s8_ansi(image2d_t* in_image, image2d_t* filter, int8_t *out_value, int count_x, int count_y, int shift);
esp_err_t dspi_dotprod_u8_ansi(image2d_t* in_image, image2d_t* filter, uint8_t *out_value, int count_x, int count_y, int shift);

esp_err_t dspi_dotprod_s16_aes3(image2d_t* in_image, image2d_t* filter, int16_t *out_value, int count_x, int count_y, int shift);
esp_err_t dspi_dotprod_u16_aes3(image2d_t* in_image, image2d_t* filter, uint16_t *out_value, int count_x, int count_y, int shift);
esp_err_t dspi_dotprod_s8_aes3(image2d_t* in_image, image2d_t* filter, int8_t *out_value, int count_x, int count_y, int shift);
esp_err_t dspi_dotprod_u8_aes3(image2d_t* in_image, image2d_t* filter, uint8_t *out_value, int count_x, int count_y, int shift);


/**@}*/ 

/**@{*/
/**
 * @brief      dot product of two images with input offset
 * Dot product calculation for two floating point images: *out_value += (image[i*...] + offset) * src2[i*...]); i= [0..count_x*count_y)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform. 
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] in_image  descriptor of the image
 * @param[in] filter  descriptor of the filter
 * @param[out] out_value   pointer to the output value
 * @param[in] count_x amount of samples by X axis  (count_x*step_X <= widdth)
 * @param[in] count_y amount of samples by Y axis (count_y*step_Y  <= height)
 * @param[in] offset - input offset value.
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspi_dotprod_off_f32_ansi(image2d_t* in_image, image2d_t* filter, float *out_value, int count_x, int count_y, float offset);
/**@}*/ 

/**@{*/
/**
 * @brief      dot product of two images with input offset
 * Dot product calculation for two floating point images: *out_value += (image[i*...] + offset) * src2[i*...]); i= [0..count_x*count_y)
 * The extension (_ansi) use ANSI C and could be compiled and run on any platform. 
 * The extension (_ae32) is optimized for ESP32 chip.
 *
 * @param[in] in_image  descriptor of the image
 * @param[in] filter  descriptor of the filter
 * @param[out] out_value   pointer to the output value
 * @param[in] count_x amount of samples by X axis  (count_x*step_X <= widdth)
 * @param[in] count_y amount of samples by Y axis (count_y*step_Y  <= height)
 * @param[in] shift - result shift to right, by default must be 15 for int16_t or 7 for int8_t 
 * @param[in] offset - input offset value.
 * @return
 *      - ESP_OK on success
 *      - One of the error codes from DSP library
 */
esp_err_t dspi_dotprod_off_s16_ansi(image2d_t* in_image, image2d_t* filter, int16_t *out_value, int count_x, int count_y, int shift, int16_t offset);
esp_err_t dspi_dotprod_off_u16_ansi(image2d_t* in_image, image2d_t* filter, uint16_t *out_value, int count_x, int count_y, int shift, uint16_t offset);
esp_err_t dspi_dotprod_off_s8_ansi(image2d_t* in_image, image2d_t* filter, int8_t *out_value, int count_x, int count_y, int shift, int8_t offset);
esp_err_t dspi_dotprod_off_u8_ansi(image2d_t* in_image, image2d_t* filter, uint8_t *out_value, int count_x, int count_y, int shift, uint8_t offset);

esp_err_t dspi_dotprod_off_s16_aes3(image2d_t* in_image, image2d_t* filter, int16_t *out_value, int count_x, int count_y, int shift, int16_t offset);
esp_err_t dspi_dotprod_off_u16_aes3(image2d_t* in_image, image2d_t* filter, uint16_t *out_value, int count_x, int count_y, int shift, uint16_t offset);
esp_err_t dspi_dotprod_off_s8_aes3(image2d_t* in_image, image2d_t* filter, int8_t *out_value, int count_x, int count_y, int shift, int8_t offset);
esp_err_t dspi_dotprod_off_u8_aes3(image2d_t* in_image, image2d_t* filter, uint8_t *out_value, int count_x, int count_y, int shift, uint8_t offset);
/**@}*/ 


#ifdef __cplusplus
}
#endif


#ifdef CONFIG_DSP_OPTIMIZED
#define dspi_dotprod_f32 dspi_dotprod_f32_ansi
#define dspi_dotprod_off_f32 dspi_dotprod_off_f32_ansi
    #if (dspi_dotprod_aes3_enabled == 1)
    #define dspi_dotprod_s16 dspi_dotprod_s16_aes3
    #define dspi_dotprod_u16 dspi_dotprod_u16_aes3
    #define dspi_dotprod_s8 dspi_dotprod_s8_aes3
    #define dspi_dotprod_u8 dspi_dotprod_u8_aes3
    #define dspi_dotprod_off_s16 dspi_dotprod_off_s16_aes3
    #define dspi_dotprod_off_s8 dspi_dotprod_off_s8_aes3
    #define dspi_dotprod_off_u16 dspi_dotprod_off_u16_aes3
    #define dspi_dotprod_off_u8 dspi_dotprod_off_u8_aes3
    #else
    #define dspi_dotprod_s16 dspi_dotprod_s16_ansi
    #define dspi_dotprod_s8 dspi_dotprod_s8_ansi
    #define dspi_dotprod_u16 dspi_dotprod_u16_ansi
    #define dspi_dotprod_u8 dspi_dotprod_u8_ansi
    #define dspi_dotprod_off_s16 dspi_dotprod_off_s16_ansi
    #define dspi_dotprod_off_s8 dspi_dotprod_off_s8_ansi
    #define dspi_dotprod_off_u16 dspi_dotprod_off_u16_ansi
    #define dspi_dotprod_off_u8 dspi_dotprod_off_u8_ansi
    #endif
#endif
#ifdef CONFIG_DSP_ANSI
#define dspi_dotprod_f32 dspi_dotprod_f32_ansi
#define dspi_dotprod_off_f32 dspi_dotprod_off_f32_ansi
#define dspi_dotprod_s16 dspi_dotprod_s16_ansi
#define dspi_dotprod_s8 dspi_dotprod_s8_ansi
#define dspi_dotprod_off_s16 dspi_dotprod_off_s16_ansi
#define dspi_dotprod_off_s8 dspi_dotprod_off_s8_ansi
#define dspi_dotprod_u16 dspi_dotprod_u16_ansi
#define dspi_dotprod_u8 dspi_dotprod_u8_ansi
#define dspi_dotprod_off_u16 dspi_dotprod_off_u16_ansi
#define dspi_dotprod_off_u8 dspi_dotprod_off_u8_ansi
#endif


#endif // _dspi_dotprod_H_