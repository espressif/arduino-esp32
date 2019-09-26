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
    int w; // Width
    int h; // Height
    int c; // Channel
    int n; // Number, to record filter's out_channels. input and output must be 1
    int stride;
    int exponent;
    qtp_t *item;
    /******* fix end *******/
} dl_matrix3dq_t;

#ifndef DL_QTP_SHIFT
#define DL_QTP_SHIFT 15
#define DL_ITMQ(m, x, y) m->itemq[(y) + (x)*m->stride]
#define DL_QTP_RANGE ((1 << DL_QTP_SHIFT) - 1)
#define DL_QTP_MAX 32767
#define DL_QTP_MIN -32768

#define DL_QTP_EXP_NA 255 //non-applicable exponent because matrix is null

#define DL_SHIFT_AUTO 32
#endif

typedef enum
{
    DL_C_IMPL = 0,
    DL_XTENSA_IMPL = 1
} dl_conv_mode;

typedef struct
{
    int stride_x;
    int stride_y;
    dl_padding_type padding;
    dl_conv_mode mode;
    int dilate_exponent;
    int depthwise_exponent;
    int compress_exponent;
} dl_matrix3dq_mobilenet_config_t;

//
// Utility
//

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
void dl_matrix3dq_slice_copy(dl_matrix3dq_t *dst, dl_matrix3dq_t *src, int x, int y, int w, int h);

dl_matrix3d_t *dl_matrix3d_from_matrixq(dl_matrix3dq_t *m);

dl_matrix3dq_t *dl_matrixq_from_matrix3d_qmf(dl_matrix3d_t *m, int exponent);

dl_matrix3dq_t *dl_matrixq_from_matrix3d(dl_matrix3d_t *m);

qtp_t dl_matrix3dq_quant_range_exceeded_checking(int64_t value, char *location);

void dl_matrix3dq_shift_exponent(dl_matrix3dq_t *out, dl_matrix3dq_t *in, int exponent);

void dl_matrix3dq_batch_normalize(dl_matrix3dq_t *m, dl_matrix3dq_t *scale, dl_matrix3dq_t *offset);

dl_matrix3dq_t *dl_matrix3dq_add(dl_matrix3dq_t *in_1, dl_matrix3dq_t *in_2, int exponent);

//
// Activation
//
void dl_matrix3dq_relu(dl_matrix3dq_t *in);

void dl_matrix3dq_relu_clip(dl_matrix3dq_t *in, fptp_t clip);

void dl_matrix3dq_leaky_relu(dl_matrix3dq_t *in, fptp_t alpha, fptp_t clip);

void dl_matrix3dq_p_relu(dl_matrix3dq_t *in, dl_matrix3dq_t *alpha);

//
// Concat
//
dl_matrix3dq_t *dl_matrix3dq_concat(dl_matrix3dq_t *in_1,
                                    dl_matrix3dq_t *in_2);

dl_matrix3dq_t *dl_matrix3dq_concat_4(dl_matrix3dq_t *in_1,
                                      dl_matrix3dq_t *in_2,
                                      dl_matrix3dq_t *in_3,
                                      dl_matrix3dq_t *in_4);

dl_matrix3dq_t *dl_matrix3dq_concat_8(dl_matrix3dq_t *in_1,
                                      dl_matrix3dq_t *in_2,
                                      dl_matrix3dq_t *in_3,
                                      dl_matrix3dq_t *in_4,
                                      dl_matrix3dq_t *in_5,
                                      dl_matrix3dq_t *in_6,
                                      dl_matrix3dq_t *in_7,
                                      dl_matrix3dq_t *in_8);

//
// Conv 1x1
//
void dl_matrix3dqq_conv_1x1(dl_matrix3dq_t *out,
                            dl_matrix3dq_t *in,
                            dl_matrix3dq_t *filter,
                            dl_conv_mode mode);

void dl_matrix3dqq_conv_1x1_with_relu(dl_matrix3dq_t *out,
                                      dl_matrix3dq_t *in,
                                      dl_matrix3dq_t *filter,
                                      dl_conv_mode mode);

void dl_matrix3dqq_conv_1x1_with_bias(dl_matrix3dq_t *out,
                                      dl_matrix3dq_t *in,
                                      dl_matrix3dq_t *filter,
                                      dl_matrix3dq_t *bias,
                                      dl_conv_mode mode,
                                      char *name);

void dl_matrix3dqq_conv_1x1_with_prelu(dl_matrix3dq_t *out,
                                       dl_matrix3dq_t *in,
                                       dl_matrix3dq_t *filter,
                                       dl_matrix3dq_t *prelu,
                                       dl_conv_mode mode);

void dl_matrix3dqq_conv_1x1_with_bias_relu(dl_matrix3dq_t *out,
                                           dl_matrix3dq_t *in,
                                           dl_matrix3dq_t *filter,
                                           dl_matrix3dq_t *bias,
                                           dl_conv_mode mode);

void dl_matrix3duq_conv_1x1(dl_matrix3dq_t *out,
                            dl_matrix3du_t *in,
                            dl_matrix3dq_t *filter,
                            dl_conv_mode mode);

void dl_matrix3duq_conv_1x1_with_bias(dl_matrix3dq_t *out,
                                      dl_matrix3du_t *in,
                                      dl_matrix3dq_t *filter,
                                      dl_matrix3dq_t *bias,
                                      dl_conv_mode mode);

//
// Conv 3x3
//
void dl_matrix3dqq_conv_3x3_op(dl_matrix3dq_t *out,
                               dl_matrix3dq_t *in,
                               dl_matrix3dq_t *f,
                               int stride_x,
                               int stride_y);

dl_matrix3dq_t *dl_matrix3dqq_conv_3x3(dl_matrix3dq_t *in,
                                       dl_matrix3dq_t *filter,
                                       int stride_x,
                                       int stride_y,
                                       dl_padding_type padding,
                                       int exponent);

dl_matrix3dq_t *dl_matrix3dqq_conv_3x3_with_bias(dl_matrix3dq_t *in,
                                                 dl_matrix3dq_t *f,
                                                 dl_matrix3dq_t *bias,
                                                 int stride_x,
                                                 int stride_y,
                                                 dl_padding_type padding,
                                                 int exponent,
                                                 int relu);

dl_matrix3dq_t *dl_matrix3duq_conv_3x3_with_bias(dl_matrix3du_t *in,
                                                 dl_matrix3dq_t *filter,
                                                 dl_matrix3dq_t *bias,
                                                 int stride_x,
                                                 int stride_y,
                                                 dl_padding_type padding,
                                                 int exponent,
                                                 char *name);

dl_matrix3dq_t *dl_matrix3duq_conv_3x3_with_bias_prelu(dl_matrix3du_t *in,
                                                       dl_matrix3dq_t *filter,
                                                       dl_matrix3dq_t *bias,
                                                       dl_matrix3dq_t *prelu,
                                                       int stride_x,
                                                       int stride_y,
                                                       dl_padding_type padding,
                                                       int exponent,
                                                       char *name);

//
// Conv common
//

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
dl_matrix3dq_t *dl_matrix3dqq_conv_common(dl_matrix3dq_t *in,
                                          dl_matrix3dq_t *filter,
                                          dl_matrix3dq_t *bias,
                                          int stride_x,
                                          int stride_y,
                                          dl_padding_type padding,
                                          int exponent,
                                          dl_conv_mode mode);

dl_matrix3dq_t *dl_matrix3duq_conv_common(dl_matrix3du_t *in,
                                          dl_matrix3dq_t *filter,
                                          dl_matrix3dq_t *bias,
                                          int stride_x,
                                          int stride_y,
                                          dl_padding_type padding,
                                          int exponent,
                                          dl_conv_mode mode);

//
// Depthwise 3x3
//
dl_matrix3dq_t *dl_matrix3duq_depthwise_conv_3x3(dl_matrix3du_t *in,
                                                 dl_matrix3dq_t *filter,
                                                 int stride_x,
                                                 int stride_y,
                                                 dl_padding_type padding,
                                                 int exponent);

dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3(dl_matrix3dq_t *in,
                                                 dl_matrix3dq_t *filter,
                                                 int stride_x,
                                                 int stride_y,
                                                 dl_padding_type padding,
                                                 int exponent);

#if CONFIG_DEVELOPING_CODE
dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3_2(dl_matrix3dq_t *in,
                                                   dl_matrix3dq_t *filter,
                                                   int stride_x,
                                                   int stride_y,
                                                   dl_padding_type padding,
                                                   int exponent);

dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3_3(dl_matrix3dq_t *in,
                                                   dl_matrix3dq_t *filter,
                                                   int stride_x,
                                                   int stride_y,
                                                   dl_padding_type padding,
                                                   int exponent);
#endif

dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3_with_bias(dl_matrix3dq_t *in,
                                                           dl_matrix3dq_t *f,
                                                           dl_matrix3dq_t *bias,
                                                           int stride_x,
                                                           int stride_y,
                                                           dl_padding_type padding,
                                                           int exponent,
                                                           int relu);

dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3_with_prelu(dl_matrix3dq_t *in,
                                                            dl_matrix3dq_t *filter,
                                                            dl_matrix3dq_t *prelu,
                                                            int stride_x,
                                                            int stride_y,
                                                            dl_padding_type padding,
                                                            int exponent);

dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3s1_with_bias(dl_matrix3dq_t *in,
                                                             dl_matrix3dq_t *f,
                                                             dl_matrix3dq_t *bias,
                                                             dl_padding_type padding,
                                                             int exponent,
                                                             int relu);

//
// Depthwise Common
//
#if CONFIG_DEVELOPING_CODE
dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_common(dl_matrix3dq_t *in,
                                                    dl_matrix3dq_t *filter,
                                                    int stride_x,
                                                    int stride_y,
                                                    dl_padding_type padding,
                                                    int exponent,
                                                    dl_conv_mode mode);

dl_matrix3dq_t *dl_matrix3duq_depthwise_conv_common(dl_matrix3du_t *in,
                                                    dl_matrix3dq_t *filter,
                                                    int stride_x,
                                                    int stride_y,
                                                    dl_padding_type padding,
                                                    int exponent,
                                                    dl_conv_mode mode);
#endif

//
// Dot Product
//

void dl_matrix3dqq_dot_product(dl_matrix3dq_t *out,
                               dl_matrix3dq_t *in,
                               dl_matrix3dq_t *filter,
                               dl_conv_mode mode);

//
// FC
//

void dl_matrix3dqq_fc(dl_matrix3dq_t *out,
                      dl_matrix3dq_t *in,
                      dl_matrix3dq_t *filter,
                      dl_conv_mode mode);

void dl_matrix3dqq_fc_with_bias(dl_matrix3dq_t *out,
                                dl_matrix3dq_t *in,
                                dl_matrix3dq_t *filter,
                                dl_matrix3dq_t *bias,
                                dl_conv_mode mode,
                                char *name);

//
// Mobilefaceblock
//

dl_matrix3dq_t *dl_matrix3dqq_mobilefaceblock_split(dl_matrix3dq_t *in,
                                                    dl_matrix3dq_t *pw_1,
                                                    dl_matrix3dq_t *pw_2,
                                                    dl_matrix3dq_t *pw_bias,
                                                    dl_matrix3dq_t *dw,
                                                    dl_matrix3dq_t *dw_bias,
                                                    dl_matrix3dq_t *pw_linear_1,
                                                    dl_matrix3dq_t *pw_linear_2,
                                                    dl_matrix3dq_t *pw_linear_bias,
                                                    int pw_exponent,
                                                    int dw_exponent,
                                                    int pw_linear_exponent,
                                                    int stride_x,
                                                    int stride_y,
                                                    dl_padding_type padding,
                                                    dl_conv_mode mode,
                                                    int shortcut);

dl_matrix3dq_t *dl_matrix3dqq_mobilefaceblock(dl_matrix3dq_t *in,
                                              dl_matrix3dq_t *pw,
                                              dl_matrix3dq_t *pw_bias,
                                              dl_matrix3dq_t *dw,
                                              dl_matrix3dq_t *dw_bias,
                                              dl_matrix3dq_t *pw_linear,
                                              dl_matrix3dq_t *pw_linear_bias,
                                              int pw_exponent,
                                              int dw_exponent,
                                              int pw_linear_exponent,
                                              int stride_x,
                                              int stride_y,
                                              dl_padding_type padding,
                                              dl_conv_mode mode,
                                              int shortcut);

//
// Mobilenet
//

dl_matrix3dq_t *dl_matrix3dqq_mobilenet(dl_matrix3dq_t *in,
                                        dl_matrix3dq_t *dilate,
                                        dl_matrix3dq_t *dilate_prelu,
                                        dl_matrix3dq_t *depthwise,
                                        dl_matrix3dq_t *depth_prelu,
                                        dl_matrix3dq_t *compress,
                                        dl_matrix3dq_t *bias,
                                        dl_matrix3dq_mobilenet_config_t config,
                                        char *name);

dl_matrix3dq_t *dl_matrix3duq_mobilenet(dl_matrix3du_t *in,
                                        dl_matrix3dq_t *dilate,
                                        dl_matrix3dq_t *dilate_prelu,
                                        dl_matrix3dq_t *depthwise,
                                        dl_matrix3dq_t *depth_prelu,
                                        dl_matrix3dq_t *compress,
                                        dl_matrix3dq_t *bias,
                                        dl_matrix3dq_mobilenet_config_t config,
                                        char *name);

//
// Padding
//

dl_error_type dl_matrix3dqq_padding(dl_matrix3dq_t **padded_in,
                                    dl_matrix3dq_t **out,
                                    dl_matrix3dq_t *in,
                                    int out_c,
                                    int stride_x,
                                    int stride_y,
                                    int padding,
                                    int exponent);

dl_error_type dl_matrix3duq_padding(dl_matrix3du_t **padded_in,
                                    dl_matrix3dq_t **out,
                                    dl_matrix3du_t *in,
                                    int out_c,
                                    int stride_x,
                                    int stride_y,
                                    int padding,
                                    int exponent);

//
// Pooling
//

dl_matrix3dq_t *dl_matrix3dq_global_pool(dl_matrix3dq_t *in);
