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
#include "hd_model.h"
#include "hp_model.h"

#define INPUT_EXPONENT -10
#define SCORE_THRESHOLD 0.5
#define NMS_THRESHOLD 0.45

#if CONFIG_HD_LITE1
  #define HP_TARGET_SIZE 128 
#else
  #define HP_TARGET_SIZE 128
#endif

    typedef struct
    {
        int target_size;          /*!< The input size of hand detection network */
        fptp_t score_threshold;   /*!< score threshold， used to filter candidates by score */
        fptp_t nms_threshold;     /*!< nms threshold， used to filter out overlapping boxes */
    } hd_config_t;

    /**
     * @brief Get the default hand detection network configuration
     * 
     * @return hd_config_t The default configuration
     */
    static inline hd_config_t hd_init_config()
    {
        hd_config_t hd_config;
        hd_config.target_size = 96;
        hd_config.score_threshold = SCORE_THRESHOLD;
        hd_config.nms_threshold = NMS_THRESHOLD;
        return hd_config;
    }
    
    typedef struct tag_od_box_list
    {
        fptp_t *score;         /*!< The confidence score of the class corresponding to the box */
        qtp_t *cls;            /*!< The class corresponding to the box */
        box_t *box;            /*!< (x1, y1, x2, y2) of the boxes */
        int len;               /*!< The number of the boxes */
    } od_box_array_t;

    typedef struct tag_od_image_box
    {
        struct tag_od_image_box *next;     /*!< Next od_image_box_t */
        fptp_t score;                      /*!< The confidence score of the class corresponding to the box */
        qtp_t cls;                         /*!< The class corresponding to the box */
        box_t box;                         /*!< (x1, y1, x2, y2) of the boxes */
    } od_image_box_t;

    typedef struct tag_od_image_list
    {
        od_image_box_t *head;              /*!< The current head of the od_image_list */
        od_image_box_t *origin_head;       /*!< The original head of the od_image_list */
        int len;                           /*!< Length of the od_image_list */
    } od_image_list_t;

    /**
     * @brief Sort the resulting box lists by their confidence score.
     * 
     * @param image_sorted_list      The sorted box list.
     * @param insert_list            The box list that have not been sorted.
     */
    void od_image_sort_insert_by_score(od_image_list_t *image_sorted_list, const od_image_list_t *insert_list);
    
    /**
     * @brief Filter out the resulting boxes whose confidence score is lower than the threshold and convert the boxes to the actual boxes on the original image.((x, y, w, h) -> (x1, y1, x2, y2)) 
     * 
     * @param score                Confidence score of the boxes.
     * @param cls                  Class of the boxes. 
     * @param boxes                (x, y, w, h) of the boxes. x and y are the center coordinates. 
     * @param height               Height of the detection output feature map.
     * @param width                Width of the detection output feature map. 
     * @param anchor_number        Anchor number of the detection output feature map. 
     * @param score_threshold      Threshold of the confidence score.
     * @param resize_scale         Resize scale: target_size/orignal_size.
     * @param padding_w            Width padding in preporcess.
     * @param padding_h            Height padding in preporcess.
     * @return od_image_list_t*    Resulting valid boxes.
     */
    od_image_list_t *od_image_get_valid_boxes(fptp_t *score,
                                    fptp_t *cls,
                                    fptp_t *boxes,
                                    int height,
                                    int width,
                                    int anchor_number,
                                    fptp_t score_threshold,
                                    fptp_t resize_scale,
                                    int padding_w,
                                    int padding_h);
    
    /**
     * @brief Run NMS algorithm 
     * 
     * @param image_list        The input boxes list
     * @param nms_threshold     NMS threshold
     */
    void od_image_nms_process(od_image_list_t *image_list, fptp_t nms_threshold);

    /**
     * @brief Do hand detection, return box infomation.
     * 
     * @param image              Image matrix, rgb888 format
     * @param hd_config          Configuration of hand detection 
     * @return od_box_array_t*   A list of boxes, score and class.
     */
    od_box_array_t *hand_detection_forward(dl_matrix3du_t *image, hd_config_t hd_config);

    /**
     * @brief Do hand pose estimation, return 21 landmarks of each hand.
     * 
     * @param image              Image matrix, rgb888 format
     * @param od_boxes           The output of the hand detection network
     * @param target_size        The input size of hand pose estimation network
     * @return dl_matrix3d_t*    The coordinates of 21 landmarks on the input image for each hand, size (n, 1, 21, 2)
     */
    dl_matrix3d_t *handpose_estimation_forward(dl_matrix3du_t *image, od_box_array_t *od_boxes, int target_size);

#if __cplusplus
}
#endif
