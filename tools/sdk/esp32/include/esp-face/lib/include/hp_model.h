#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "dl_lib_matrix3d.h"
#include "dl_lib_matrix3dq.h"

    /**
     * @brief Forward the hand pose estimation process with hp_nano1_ls16 model. Calculate in quantization.
     * 
     * @param in                 A normalized image matrix in rgb888 format, its size is (1, 128, 128, 3).
     * @param mode               0: C implement; 1: handwrite xtensa instruction implement
     * @return dl_matrix3d_t*    The resulting hand joint point coordinates, the size is (1, 1, 21, 2)
     */
    dl_matrix3d_t *hp_nano1_ls16_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    /**
     * @brief Forward the hand pose estimation process with hp_lite1 model. Calculate in quantization.
     * 
     * @param in                 A normalized image matrix in rgb888 format, its size is (1, 128, 128, 3).
     * @param mode               0: C implement; 1: handwrite xtensa instruction implement
     * @return dl_matrix3d_t*    The resulting hand joint point coordinates, the size is (1, 1, 21, 2)
     */
    dl_matrix3d_t *hp_lite1_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    /**
     * @brief Test the result of hand pose estimation model.
     * 
     */
    void hp_test();

    /**
     * @brief Test the forward time of hand pose estimation model.
     * 
     */
    void hp_time_test();

#if __cplusplus
}
#endif