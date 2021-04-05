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
#include "detection.h"
// Include models
#include "cat_face_3.h"

    /**
     * @brief update detection hyperparameter 
     * 
     * @param model             The detection model
     * @param resize_scale      The resize scale of input image
     * @param score_threshold   Score threshold, used to filter candidates by score
     * @param nms_threshold     NMS threshold, used to filter out overlapping boxes
     * @param image_height      Input image height
     * @param image_width       Input image width
     */
    void update_detection_model(detection_model_t *model, fptp_t resize_scale, fptp_t score_threshold, fptp_t nms_threshold, int image_height, int image_width);

    /**
     * @brief 
     * 
     * @param image             The input image
     * @param model             A 'detection_model_t' type point of detection model
     * @return box_array_t*     The detection result with box and corresponding score and category
     */
    box_array_t *detect_object(dl_matrix3du_t *image, detection_model_t *model);

#if __cplusplus
}
#endif
