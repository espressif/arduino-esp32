#pragma once
#include "dl_lib_matrix3d.h"

typedef int16_t qtp_t;

/*
 * Matrix for 3d
 * @Warning: the sequence of variables is fixed, cannot be modified, otherwise there will be errors in esp_dsp_dot_float
 */
typedef struct
{
    /******* fix start *******/
    int w;  // Width
    int h;  // Height
    int c;  // Channel
    int n;  // Number, to record filter's out_channels. input and output must be 1
    int stride;
    int exponent;
    qtp_t *item;
    /******* fix end *******/
} dl_matrix3dq_t;

#define DL_QTP_SHIFT 15
#define DL_QTP_RANGE ((1<<DL_QTP_SHIFT)-1)
//#define DL_ITMQ(m, x, y) m->itemq[(y)+(x)*m->stride]
#define DL_QTP_EXP_NA 255 //non-applicable exponent because matrix is null

#define DL_SHIFT_AUTO 32

/*
 * @brief Allocate a 3D matrix
 *
 * @param n,w,h,c   number, width, height, channel
 * @return 3d matrix
 */
dl_matrix3dq_t *dl_matrix3dq_alloc(int n, int w, int h, int c, int e);

/*
 * @brief Free a 3D matrix
 *
 * @param m matrix
 */
void dl_matrix3dq_free(dl_matrix3dq_t *m);

/**
 * @brief Zero out the matrix
 * Sets all entries in the matrix to 0.
 *
 * @param m     Matrix to zero
 */

 dl_matrix3d_t *dl_matrix3d_from_matrixq(dl_matrix3dq_t *m);
 dl_matrix3dq_t *dl_matrixq_from_matrix3d_qmf(dl_matrix3d_t *m,int exponent);
 dl_matrix3dq_t *dl_matrixq_from_matrix3d(dl_matrix3d_t *m);
/**
 * @brief Copy a range of items from an existing matrix to a preallocated matrix
 *
 * @param in    Old matrix (with foreign data) to re-use. Passing NULL will allocate a new matrix.
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 * @return The resulting slice matrix
 */
void dl_matrix3dq_slice_copy (dl_matrix3dq_t *dst, dl_matrix3dq_t *src, int x, int y, int w, int h);


/**
 * @brief Do a general CNN layer pass, dimension is (number, width, height, channel)
 *
 * @param in             Input image
 * @param filter         Weights of the neurons
 * @param bias           Bias for the CNN layer.
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param padding        One of VALID or SAME
 * @param mode           Do convolution using C implement or xtensa implement, 0 or 1, with respect.
 *                       If ESP_PLATFORM is not defined, this value is not used.
 * @return               The result of CNN layer.
 */
dl_matrix3dq_t *dl_matrix3dq_fc (dl_matrix3dq_t *in, dl_matrix3dq_t *filter, dl_matrix3dq_t *bias, int exponent,int mode);

dl_matrix3dq_t *dl_matrix3dq_conv (dl_matrix3dq_t *in, dl_matrix3dq_t *filter, dl_matrix3dq_t *bias,
                                    int stride_x, int stride_y, int padding, int exponent, int mode);
dl_matrix3dq_t *dl_matrix3dq_conv_normal (dl_matrix3dq_t *in, dl_matrix3dq_t *filter, dl_matrix3dq_t *bias,
                                    int stride_x, int stride_y, int padding, int exponent, int mode);

/**
 * @brief Print the matrix3d items
 *
 * @param m              dl_matrix3d_t to be printed
 * @param message        name of matrix
 */
void dl_matrix3dq_print (dl_matrix3dq_t *m, char *message);

dl_matrix3dq_t *dl_matrix3dq_depthwise_conv (dl_matrix3dq_t *in, dl_matrix3dq_t *filter,
                                    int stride_x, int stride_y, int padding, int exponent, int mode);

void dl_matrix3dq_relu (dl_matrix3dq_t *m, fptp_t clip);



dl_matrix3dq_t *dl_matrix3dq_global_pool (dl_matrix3dq_t *in);
void dl_matrix3dq_batch_normalize (dl_matrix3dq_t *m, dl_matrix3dq_t *scale, dl_matrix3dq_t *offset);
dl_matrix3dq_t *dl_matrix3dq_add (dl_matrix3dq_t *in_1, dl_matrix3dq_t *in_2, int exponent);
void dl_matrix3dq_relu_std (dl_matrix3dq_t *m);
dl_matrix3dq_t *dl_matrix3dq_mobilefaceblock (void *in, dl_matrix3dq_t *pw, dl_matrix3dq_t *pw_bn_scale,dl_matrix3dq_t *pw_bn_offset,
                                        dl_matrix3dq_t *dw, dl_matrix3dq_t *dw_bn_scale,dl_matrix3dq_t *dw_bn_offset,
                                        dl_matrix3dq_t *pw_linear, dl_matrix3dq_t *pw_linear_bn_scale,dl_matrix3dq_t *pw_linear_bn_offset,
                                        int pw_exponent,int dw_exponent,int pw_linear_exponent,int stride_x, int stride_y, int padding, int mode, int shortcut);

dl_matrix3dq_t *dl_matrix3dq_concat(dl_matrix3dq_t *in_1, dl_matrix3dq_t *in_2);
dl_matrix3dq_t *dl_matrix3dq_concat_4(dl_matrix3dq_t *in_1, dl_matrix3dq_t *in_2, dl_matrix3dq_t *in_3, dl_matrix3dq_t *in_4);
dl_matrix3dq_t *dl_matrix3dq_concat_8(dl_matrix3dq_t *in_1, dl_matrix3dq_t *in_2, dl_matrix3dq_t *in_3, dl_matrix3dq_t *in_4, dl_matrix3dq_t *in_5, dl_matrix3dq_t *in_6, dl_matrix3dq_t *in_7, dl_matrix3dq_t *in_8);

dl_matrix3dq_t *dl_matrix3dq_mobilefaceblock_split (void *in, dl_matrix3dq_t *pw_1, dl_matrix3dq_t *pw_2, dl_matrix3dq_t *pw_bn_scale,dl_matrix3dq_t *pw_bn_offset,
                                        dl_matrix3dq_t *dw, dl_matrix3dq_t *dw_bn_scale,dl_matrix3dq_t *dw_bn_offset,
                                        dl_matrix3dq_t *pw_linear_1, dl_matrix3dq_t *pw_linear_2, dl_matrix3dq_t *pw_linear_bn_scale,dl_matrix3dq_t *pw_linear_bn_offset,
                                        int pw_exponent,int dw_exponent,int pw_linear_exponent,int stride_x, int stride_y, int padding, int mode, int shortcut);
