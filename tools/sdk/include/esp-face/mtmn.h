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
#include "dl_lib.h"

    typedef enum
    {
        PNET = 0, /// P-Net
        RNET = 1, /// R-Net
        ONET = 2, /// O-Net
    } net_type_en;

    typedef struct
    {
        float score;          /// score threshold for filter candidates by score
        float nms;            /// nms threshold for nms process
        int candidate_number; /// candidate number limitation for each net
    } threshold_config_t;

    typedef struct
    {
        net_type_en net_type;         /// net type
        char *file_name;              /// net name
        int w;                        /// net width
        int h;                        /// net height
        threshold_config_t threshold; /// threshold of net
    } net_config_t;

    typedef struct
    {
        float min_face;                 /// the minimum size of face can be detected
        float pyramid;                  /// the pyramid scale
        threshold_config_t p_threshold; /// score, nms and candidate threshold of pnet
        threshold_config_t r_threshold; /// score, nms and candidate threshold of rnet
        threshold_config_t o_threshold; /// score, nms and candidate threshold of onet
    } mtmn_config_t;

    typedef struct
    {
        dl_matrix3d_t *category;
        dl_matrix3d_t *offset;
        dl_matrix3d_t *landmark;
    } mtmn_net_t;

    /**
     * @brief Forward the pnet process, coarse detection
     *
     * @param in        Image matrix, rgb888 format, size is 320x240
     * @return          Scores for every pixel, and box offset with respect.
     */
    mtmn_net_t *pnet(dl_matrix3du_t *in);

    /**
     * @brief Forward the rnet process, fine determine the boxes from pnet
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, and box offset with respect.
     */
    mtmn_net_t *rnet_with_score_verify(dl_matrix3du_t *in, float threshold);

    /**
     * @brief Forward the onet process, fine determine the boxes from rnet
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, box offset, and landmark with respect.
     */
    mtmn_net_t *onet_with_score_verify(dl_matrix3du_t *in, float threshold);

#ifdef __cplusplus
}
#endif
