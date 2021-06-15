/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include "dl_lib_matrix3d.h"
#include "dl_lib_matrix3dq.h"
#include "freertos/FreeRTOS.h"

    typedef struct
    {
        int resized_height;
        int resized_width;
        fptp_t y_resize_scale;
        fptp_t x_resize_scale;
        int enabled_top_k;
        fptp_t score_threshold;
        fptp_t nms_threshold;

        dl_conv_mode mode;
    } lssh_config_t;

    typedef struct
    {
        int *anchor_size;
        int stride;
        int boundary;
    } lssh_module_config_t;

    typedef struct
    {
        lssh_module_config_t *module_config;
        int number;
    } lssh_modules_config_t;

    typedef struct
    {
        dl_matrix3d_t *category;
        dl_matrix3d_t *box_offset;
        dl_matrix3d_t *landmark_offset;
    } lssh_module_result_t;

    /**
     * @brief 
     * 
     * @param value 
     */
    void lssh_module_result_free(lssh_module_result_t value);

    /**
     * @brief 
     * 
     * @param values 
     * @param length 
     */
    void lssh_module_results_free(lssh_module_result_t *values, int length);

    /////////////////////////
    //////sparse_mn_5_q//////
    /////////////////////////
    extern lssh_modules_config_t sparse_mn_5_modules_config;
    lssh_module_result_t *sparse_mn_5_q_without_landmark(dl_matrix3du_t *image, bool free_image, int enabled_top_k, dl_conv_mode mode);
    lssh_module_result_t *sparse_mn_5_q_with_landmark(dl_matrix3du_t *image, bool free_image, int enabled_top_k, dl_conv_mode mode);

#ifdef __cplusplus
}
#endif
