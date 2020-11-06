#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "dl_lib_matrix3d.h"
#include "dl_lib_matrix3dq.h"

    typedef struct
    {
        int num;              /*!< The total number of the boxes */
        dl_matrix3d_t *cls;   /*!< The class feature map corresponding to the box. size: (height, width, anchor_num, 1) */
        dl_matrix3d_t *score; /*!< The confidence score feature map of the class corresponding to the box. size: (height, width, anchor_num, 1) */
        dl_matrix3d_t *boxes; /*!< (x, y, w, h) of the boxes. x and y are the center coordinates. size:(height, width, anchor_num, 4) */
    } detection_result_t;

    /**
     * @brief Forward the hand detection process with hd_nano1 model. Calculate in quantization.
     * 
     * @param in                      A normalized image matrix in rgb888 format, its width and height must be integer multiples of 16.
     * @param mode                    0: C implement; 1: handwrite xtensa instruction implement
     * @return detection_result_t**   Detection results
     */
    detection_result_t **hd_nano1_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    /**
     * @brief Forward the hand detection process with hd_lite1 model. Calculate in quantization.
     * 
     * @param in                      A normalized image matrix in rgb888 format, its width and height must be integer multiples of 32.
     * @param mode                    0: C implement; 1: handwrite xtensa instruction implement.
     * @return detection_result_t**   Detection results.
     */
    detection_result_t **hd_lite1_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    /**
     * @brief Free the single detection result.
     * 
     * @param m     The single detection result.
     */
    void detection_result_free(detection_result_t *m);

    /**
     * @brief Free the detection result group from different feature map.
     * 
     * @param m       The detection result group
     * @param length  The number of the detection results
     */
    void detection_results_free(detection_result_t **m, int length);

    /**
     * @brief Test the result of hand detection model.
     * 
     */
    void hd_test();

    /**
     * @brief Test the forward time of hand detection model.
     * 
     */
    void hd_time_test();

#if __cplusplus
}
#endif
