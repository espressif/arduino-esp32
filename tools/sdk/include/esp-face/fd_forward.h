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

#if __cplusplus
extern "C"
{
#endif

#include "image_util.h"
#include "dl_lib_matrix3d.h"
#include "mtmn.h"

    typedef enum
    {
        FAST = 0,
        NORMAL = 1,
    } mtmn_resize_type;

    typedef struct
    {
        float score;          /// score threshold for filter candidates by score
        float nms;            /// nms threshold for nms process
        int candidate_number; /// candidate number limitation for each net
    } threshold_config_t;

    typedef struct
    {
        int w;                        /// net width
        int h;                        /// net height
        threshold_config_t threshold; /// threshold of net
    } net_config_t;

    typedef struct
    {
        float min_face;                 /// The minimum size of a detectable face
        float pyramid;                  /// The scale of the gradient scaling for the input images
        int pyramid_times;              /// The pyramid resizing times
        threshold_config_t p_threshold; /// The thresholds for P-Net. For details, see the definition of threshold_config_t
        threshold_config_t r_threshold; /// The thresholds for R-Net. For details, see the definition of threshold_config_t
        threshold_config_t o_threshold; /// The thresholds for O-Net. For details, see the definition of threshold_config_t
        mtmn_resize_type type;          /// The image resize type. 'pyramid' will lose efficacy, when 'type'==FAST.
    } mtmn_config_t;

    static inline mtmn_config_t mtmn_init_config()
    {
        mtmn_config_t mtmn_config;
        mtmn_config.type = FAST;
        mtmn_config.min_face = 80;
        mtmn_config.pyramid = 0.707;
        mtmn_config.pyramid_times = 4;
        mtmn_config.p_threshold.score = 0.6;
        mtmn_config.p_threshold.nms = 0.7;
        mtmn_config.p_threshold.candidate_number = 20;
        mtmn_config.r_threshold.score = 0.7;
        mtmn_config.r_threshold.nms = 0.7;
        mtmn_config.r_threshold.candidate_number = 10;
        mtmn_config.o_threshold.score = 0.7;
        mtmn_config.o_threshold.nms = 0.7;
        mtmn_config.o_threshold.candidate_number = 1;

        return mtmn_config;
    }

    /**
     * @brief Do MTMN face detection, return box and landmark infomation.
     * 
     * @param image_matrix      Image matrix, rgb888 format
     * @param config            Configuration of MTMN i.e. score threshold, nms threshold, candidate number threshold, pyramid, min face size
     * @return box_array_t*     A list of boxes and score.
     */
    box_array_t *face_detect(dl_matrix3du_t *image_matrix,
                             mtmn_config_t *config);

#if __cplusplus
}
#endif
