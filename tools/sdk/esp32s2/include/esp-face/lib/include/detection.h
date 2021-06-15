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

    typedef enum
    {
        Anchor_Point, /*<! Anchor point detection model*/
        Anchor_Box    /*<! Anchor box detection model */
    } detection_model_type_t;

    typedef struct
    {
        int **anchors_shape; /*<! Anchor shape of this stage */
        int stride;          /*<! Zoom in stride of this stage */
        int boundary;        /*<! Detection image low-limit of this stage */
        int project_offset;  /*<! Project offset of this stage */
    } detection_stage_config_t;

    typedef struct
    {
        dl_matrix3dq_t *score;           /*<! score feature map of this stage*/
        dl_matrix3dq_t *box_offset;      /*<! box_offset feature map of this stage*/
        dl_matrix3dq_t *landmark_offset; /*<! landmark_offset feature map of this stage */
    } detection_stage_result_t;

    typedef struct
    {
        int resized_height;    /*<! The height after resized */
        int resized_width;     /*<! The width after resized */
        fptp_t y_resize_scale; /*<! resized_height / input_height */
        fptp_t x_resize_scale; /*<! resized_width / input_width */
        qtp_t score_threshold; /*<! Score threshold of detection model */
        fptp_t nms_threshold;  /*<! NMS threshold of detection model */
        bool with_landmark;    /*<! Whether detection with landmark, true: with, false: without */
        bool free_image;       /*<! Whether free the resized image */
        int enabled_top_k;     /*<! The number of enabled stages */
    } detection_model_config_t;

    typedef struct
    {
        detection_stage_config_t *stage_config;                                                                      /*<! Configuration of each stage */
        int stage_number;                                                                                            /*<! The number of stages */
        detection_model_type_t model_type;                                                                           /*<! The type of detection model */
        detection_model_config_t model_config;                                                                       /*<! Configuration of detection model */
        detection_stage_result_t *(*op)(dl_matrix3dq_t *, detection_model_config_t *);                               /*<! The function of detection inference */
        void *(*get_boxes)(detection_stage_result_t *, detection_model_config_t *, detection_stage_config_t *, int); /*<! The function of how to get real boxes */
    } detection_model_t;

    /**
     * @brief free 'detection_stage_result_t' type value
     * 
     * @param value A 'detection_stage_result_t' type value
     */
    void free_detection_stage_result(detection_stage_result_t value);

#ifdef __cplusplus
}
#endif
