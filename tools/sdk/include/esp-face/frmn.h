#pragma once

#if __cplusplus
extern "C"
{
#endif

#include "dl_lib_matrix3d.h"
#include "dl_lib_matrix3dq.h"

    /**
     * @brief 
     * 
     * @param in 
     * @return dl_matrix3d_t* 
     */
    dl_matrix3d_t *frmn(dl_matrix3d_t *in);

    /**
     * @brief 
     * 
     * @param in 
     * @return dl_matrix3dq_t* 
     */
    dl_matrix3dq_t *frmn_q(dl_matrix3dq_t *in, dl_conv_mode mode);

    dl_matrix3dq_t *frmn2_q(dl_matrix3dq_t *in, dl_conv_mode mode);
    dl_matrix3dq_t *frmn2p_q(dl_matrix3dq_t *in, dl_conv_mode mode);
    dl_matrix3dq_t *frmn2c_q(dl_matrix3dq_t *in, dl_conv_mode mode);

#if __cplusplus
}
#endif
