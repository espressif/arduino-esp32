#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "dl_lib_matrix3d.h"
#include "dl_lib_matrix3dq.h"

    /**
     * @brief Forward the face recognition process with frmn model. Calculate in float.
     *
     * @param in    Image matrix, rgb888 format, size is 56x56, normalized
     * @return dl_matrix3d_t* Face ID feature vector, size is 512
     */
    dl_matrix3d_t *frmn(dl_matrix3d_t *in);

    /**
     * @brief Forward the face recognition process with frmn model. Calculate in quantization.
     *
     * @param in    Image matrix, rgb888 format, size is 56x56, normalized
     * @param mode  0: C implement; 1: handwrite xtensa instruction implement
     * @return      Face ID feature vector, size is 512
     */
    dl_matrix3dq_t *frmn_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    /**
     * @brief Forward the face recognition process with frmn2p model. Calculate in quantization.
     *
     * @param in    Image matrix, rgb888 format, size is 56x56, normalized
     * @param mode  0: C implement; 1: handwrite xtensa instruction implement
     * @return      Face ID feature vector, size is 512
     */
    dl_matrix3dq_t *frmn2p_q(dl_matrix3dq_t *in, dl_conv_mode mode);


    dl_matrix3dq_t *mfn56_42m_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    dl_matrix3dq_t *mfn56_72m_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    dl_matrix3dq_t *mfn56_112m_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    dl_matrix3dq_t *mfn56_156m_q(dl_matrix3dq_t *in, dl_conv_mode mode);

#if __cplusplus
}
#endif
