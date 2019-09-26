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

    typedef struct
    {
        dl_matrix3d_t *category;
        dl_matrix3d_t *offset;
        dl_matrix3d_t *landmark;
    } mtmn_net_t;

    /**
     * @brief Free a mtmn_net_t
     *
     * @param p         A mtmn_net_t pointer
     */
    void mtmn_net_t_free(mtmn_net_t *p);

    /**
     * @brief Forward the pnet process, coarse detection. Calculate in float.
     *
     * @param in        Image matrix, rgb888 format, size is 320x240
     * @return          Scores for every pixel, and box offset with respect.
     */
    mtmn_net_t *pnet_lite_f(dl_matrix3du_t *in);

    /**
     * @brief Forward the rnet process, fine determine the boxes from pnet. Calculate in float.
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, and box offset with respect.
     */
    mtmn_net_t *rnet_lite_f_with_score_verify(dl_matrix3du_t *in, float threshold);

    /**
     * @brief Forward the onet process, fine determine the boxes from rnet. Calculate in float.
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, box offset, and landmark with respect.
     */
    mtmn_net_t *onet_lite_f_with_score_verify(dl_matrix3du_t *in, float threshold);

    /**
     * @brief Forward the pnet process, coarse detection. Calculate in quantization.
     *
     * @param in        Image matrix, rgb888 format, size is 320x240
     * @return          Scores for every pixel, and box offset with respect.
     */
    mtmn_net_t *pnet_lite_q(dl_matrix3du_t *in, dl_conv_mode mode);

    /**
     * @brief Forward the rnet process, fine determine the boxes from pnet. Calculate in quantization.
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, and box offset with respect.
     */
    mtmn_net_t *rnet_lite_q_with_score_verify(dl_matrix3du_t *in, float threshold, dl_conv_mode mode);

    /**
     * @brief Forward the onet process, fine determine the boxes from rnet. Calculate in quantization.
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, box offset, and landmark with respect.
     */
    mtmn_net_t *onet_lite_q_with_score_verify(dl_matrix3du_t *in, float threshold, dl_conv_mode mode);

    /**
     * @brief Forward the pnet process, coarse detection. Calculate in quantization.
     *
     * @param in        Image matrix, rgb888 format, size is 320x240
     * @return          Scores for every pixel, and box offset with respect.
     */
    mtmn_net_t *pnet_heavy_q(dl_matrix3du_t *in, dl_conv_mode mode);

    /**
     * @brief Forward the rnet process, fine determine the boxes from pnet. Calculate in quantization.
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, and box offset with respect.
     */
    mtmn_net_t *rnet_heavy_q_with_score_verify(dl_matrix3du_t *in, float threshold, dl_conv_mode mode);

    /**
     * @brief Forward the onet process, fine determine the boxes from rnet. Calculate in quantization.
     *
     * @param in        Image matrix, rgb888 format
     * @param threshold Score threshold to detect human face
     * @return          Scores for every box, box offset, and landmark with respect.
     */
    mtmn_net_t *onet_heavy_q_with_score_verify(dl_matrix3du_t *in, float threshold, dl_conv_mode mode);

#ifdef __cplusplus
}
#endif
