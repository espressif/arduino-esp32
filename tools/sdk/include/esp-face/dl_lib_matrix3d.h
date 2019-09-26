#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef float fptp_t;
typedef uint8_t uc_t;

typedef enum
{
    DL_SUCCESS = 0,
    DL_FAIL = 1,
} dl_error_type;

typedef enum
{
    PADDING_VALID = 0,
    PADDING_SAME = 1,
} dl_padding_type;

/*
 * Matrix for 3d
 * @Warning: the sequence of variables is fixed, cannot be modified, otherwise there will be errors in esp_dsp_dot_float
 */
typedef struct
{
    int w;          /*!< Width */
    int h;          /*!< Height */
    int c;          /*!< Channel */
    int n;          /*!< Number of filter, input and output must be 1 */
    int stride;     /*!< Step between lines */
    fptp_t *item;   /*!< Data */
} dl_matrix3d_t;

typedef struct
{
    int w;          /*!< Width */
    int h;          /*!< Height */
    int c;          /*!< Channel */
    int n;          /*!< Number of filter, input and output must be 1 */
    int stride;     /*!< Step between lines */
    uc_t *item;     /*!< Data */
} dl_matrix3du_t;

typedef struct
{
    int stride_x;
    int stride_y;
    dl_padding_type padding;
} dl_matrix3d_mobilenet_config_t;

/*
 * @brief Allocate a 3D matrix with float items, the access sequence is NHWC
 *
 * @param n     Number of matrix3d, for filters it is out channels, for others it is 1
 * @param w     Width of matrix3d
 * @param h     Height of matrix3d
 * @param c     Channel of matrix3d
 * @return      3d matrix
 */
dl_matrix3d_t *dl_matrix3d_alloc(int n, int w, int h, int c);

/*
 * @brief Allocate a 3D matrix with 8-bits items, the access sequence is NHWC
 *
 * @param n     Number of matrix3d, for filters it is out channels, for others it is 1
 * @param w     Width of matrix3d
 * @param h     Height of matrix3d
 * @param c     Channel of matrix3d
 * @return      3d matrix
 */
dl_matrix3du_t *dl_matrix3du_alloc(int n, int w, int h, int c);

/*
 * @brief Free a matrix3d
 *
 * @param m matrix3d with float items
 */
void dl_matrix3d_free(dl_matrix3d_t *m);

/*
 * @brief Free a matrix3d
 *
 * @param m matrix3d with 8-bits items
 */
void dl_matrix3du_free(dl_matrix3du_t *m);

/*
 * @brief Dot product with a vector and matrix
 *
 * @param out   Space to put the result
 * @param in    input vector
 * @param f     filter matrix
 */
void dl_matrix3dff_dot_product(dl_matrix3d_t *out, dl_matrix3d_t *in, dl_matrix3d_t *f);

/**
 * @brief Do a softmax operation on a matrix3d
 *
 * @param in        Input matrix3d
 */
void dl_matrix3d_softmax(dl_matrix3d_t *m);

/**
 * @brief Copy a range of float items from an existing matrix to a preallocated matrix
 *
 * @param dst   The destination slice matrix
 * @param src   The source matrix to slice
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 */
void dl_matrix3d_slice_copy(dl_matrix3d_t *dst,
                            dl_matrix3d_t *src,
                            int x,
                            int y,
                            int w,
                            int h);

/**
 * @brief Copy a range of 8-bits items from an existing matrix to a preallocated matrix
 *
 * @param dst   The destination slice matrix
 * @param src   The source matrix to slice
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 */
void dl_matrix3du_slice_copy(dl_matrix3du_t *dst,
                             dl_matrix3du_t *src,
                             int x,
                             int y,
                             int w,
                             int h);

/**
 * @brief Do a general CNN layer pass, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3d
 * @param filter         Weights of the neurons
 * @param bias           Bias for the CNN layer
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param padding        One of VALID or SAME
 * @param mode           Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                       If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return               The result of CNN layer
 */
dl_matrix3d_t *dl_matrix3d_conv(dl_matrix3d_t *in,
                                dl_matrix3d_t *filter,
                                dl_matrix3d_t *bias,
                                int stride_x,
                                int stride_y,
                                int padding,
                                int mode);

/**
 * @brief Do a general CNN layer pass, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3d
 * @param filter         Weights of the neurons
 * @param bias           Bias for the CNN layer
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param padding        One of VALID or SAME
 * @param mode           Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                       If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return               The result of CNN layer
 */

/**
 * @brief Do a global average pooling layer pass, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3d
 *
 * @return               The result of global average pooling layer
 */
dl_matrix3d_t *dl_matrix3d_global_pool(dl_matrix3d_t *in);

/**
 * @brief Do a batch normalization operation, update the input matrix3d: input = input * scale + offset
 *
 * @param m              Input matrix3d
 * @param scale          scale matrix3d,  scale = gamma/((moving_variance+sigma)^(1/2))
 * @param Offset         Offset matrix3d, offset = beta-(moving_mean*gamma/((moving_variance+sigma)^(1/2)))
 */
void dl_matrix3d_batch_normalize(dl_matrix3d_t *m,
                                 dl_matrix3d_t *scale,
                                 dl_matrix3d_t *offset);

/**
 * @brief Add a pair of matrix3d item-by-item: res=in_1+in_2
 *
 * @param in_1           First Floating point input matrix3d
 * @param in_2           Second Floating point input matrix3d
 *
 * @return               Added data
 */
dl_matrix3d_t *dl_matrix3d_add(dl_matrix3d_t *in_1, dl_matrix3d_t *in_2);

/**
 * @brief Concatenate the channels of two matrix3ds into a new matrix3d
 *
 * @param in_1           First Floating point input matrix3d
 * @param in_2           Second Floating point input matrix3d
 *
 * @return               A newly allocated matrix3d with as avlues in_1|in_2
 */
dl_matrix3d_t *dl_matrix3d_concat(dl_matrix3d_t *in_1, dl_matrix3d_t *in_2);

/**
 * @brief Concatenate the channels of four matrix3ds into a new matrix3d
 *
 * @param in_1           First Floating point input matrix3d
 * @param in_2           Second Floating point input matrix3d
 * @param in_3           Third Floating point input matrix3d
 * @param in_4           Fourth Floating point input matrix3d
 *
 * @return               A newly allocated matrix3d with as avlues in_1|in_2|in_3|in_4
 */
dl_matrix3d_t *dl_matrix3d_concat_4(dl_matrix3d_t *in_1,
                                    dl_matrix3d_t *in_2,
                                    dl_matrix3d_t *in_3,
                                    dl_matrix3d_t *in_4);

/**
 * @brief Concatenate the channels of eight matrix3ds into a new matrix3d
 *
 * @param in_1           First Floating point input matrix3d
 * @param in_2           Second Floating point input matrix3d
 * @param in_3           Third Floating point input matrix3d
 * @param in_4           Fourth Floating point input matrix3d
 * @param in_5           Fifth Floating point input matrix3d
 * @param in_6           Sixth Floating point input matrix3d
 * @param in_7           Seventh Floating point input matrix3d
 * @param in_8           eighth Floating point input matrix3d
 *
 * @return               A newly allocated matrix3d with as avlues in_1|in_2|in_3|in_4|in_5|in_6|in_7|in_8
 */
dl_matrix3d_t *dl_matrix3d_concat_8(dl_matrix3d_t *in_1,
                                    dl_matrix3d_t *in_2,
                                    dl_matrix3d_t *in_3,
                                    dl_matrix3d_t *in_4,
                                    dl_matrix3d_t *in_5,
                                    dl_matrix3d_t *in_6,
                                    dl_matrix3d_t *in_7,
                                    dl_matrix3d_t *in_8);

/**
 * @brief Do a mobilefacenet block forward, dimension is (number, width, height, channel)
 *
 * @param in                    Input matrix3d
 * @param pw                    Weights of the pointwise conv layer
 * @param pw_bn_scale           The scale params of the batch_normalize layer after the pointwise conv layer
 * @param pw_bn_offset          The offset params of the batch_normalize layer after the pointwise conv layer
 * @param dw                    Weights of the depthwise conv layer
 * @param dw_bn_scale           The scale params of the batch_normalize layer after the depthwise conv layer
 * @param dw_bn_offset          The offset params of the batch_normalize layer after the depthwise conv layer
 * @param pw_linear             Weights of the pointwise linear conv layer
 * @param pw_linear_bn_scale    The scale params of the batch_normalize layer after the pointwise linear conv layer
 * @param pw_linear_bn_offset   The offset params of the batch_normalize layer after the pointwise linear conv layer
 * @param stride_x              The step length of the convolution window in x(width) direction
 * @param stride_y              The step length of the convolution window in y(height) direction
 * @param padding               One of VALID or SAME
 * @param mode                  Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                              If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return                      The result of a mobilefacenet block
 */
dl_matrix3d_t *dl_matrix3d_mobilefaceblock(dl_matrix3d_t *in,
                                           dl_matrix3d_t *pw,
                                           dl_matrix3d_t *pw_bn_scale,
                                           dl_matrix3d_t *pw_bn_offset,
                                           dl_matrix3d_t *dw,
                                           dl_matrix3d_t *dw_bn_scale,
                                           dl_matrix3d_t *dw_bn_offset,
                                           dl_matrix3d_t *pw_linear,
                                           dl_matrix3d_t *pw_linear_bn_scale,
                                           dl_matrix3d_t *pw_linear_bn_offset,
                                           int stride_x,
                                           int stride_y,
                                           int padding,
                                           int mode,
                                           int shortcut);

/**
 * @brief Do a mobilefacenet block forward with 1x1 split conv, dimension is (number, width, height, channel)
 *
 * @param in                    Input matrix3d
 * @param pw_1                  Weights of the pointwise conv layer 1
 * @param pw_2                  Weights of the pointwise conv layer 2
 * @param pw_bn_scale           The scale params of the batch_normalize layer after the pointwise conv layer
 * @param pw_bn_offset          The offset params of the batch_normalize layer after the pointwise conv layer
 * @param dw                    Weights of the depthwise conv layer
 * @param dw_bn_scale           The scale params of the batch_normalize layer after the depthwise conv layer
 * @param dw_bn_offset          The offset params of the batch_normalize layer after the depthwise conv layer
 * @param pw_linear_1           Weights of the pointwise linear conv layer 1
 * @param pw_linear_2           Weights of the pointwise linear conv layer 2
 * @param pw_linear_bn_scale    The scale params of the batch_normalize layer after the pointwise linear conv layer
 * @param pw_linear_bn_offset   The offset params of the batch_normalize layer after the pointwise linear conv layer
 * @param stride_x              The step length of the convolution window in x(width) direction
 * @param stride_y              The step length of the convolution window in y(height) direction
 * @param padding               One of VALID or SAME
 * @param mode                  Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                              If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return                      The result of a mobilefacenet block
 */
dl_matrix3d_t *dl_matrix3d_mobilefaceblock_split(dl_matrix3d_t *in,
                                                 dl_matrix3d_t *pw_1,
                                                 dl_matrix3d_t *pw_2,
                                                 dl_matrix3d_t *pw_bn_scale,
                                                 dl_matrix3d_t *pw_bn_offset,
                                                 dl_matrix3d_t *dw,
                                                 dl_matrix3d_t *dw_bn_scale,
                                                 dl_matrix3d_t *dw_bn_offset,
                                                 dl_matrix3d_t *pw_linear_1,
                                                 dl_matrix3d_t *pw_linear_2,
                                                 dl_matrix3d_t *pw_linear_bn_scale,
                                                 dl_matrix3d_t *pw_linear_bn_offset,
                                                 int stride_x,
                                                 int stride_y,
                                                 int padding,
                                                 int mode,
                                                 int shortcut);

void dl_matrix3d_init_bias(dl_matrix3d_t *out, dl_matrix3d_t *bias);

void dl_matrix3d_multiply(dl_matrix3d_t *out, dl_matrix3d_t *in1, dl_matrix3d_t *in2);

//
// Activation
//

/**
 * @brief Do a standard relu operation, update the input matrix3d
 *
 * @param m        Floating point input matrix3d
 */
void dl_matrix3d_relu(dl_matrix3d_t *m);

/**
 * @brief Do a relu (Rectifier Linear Unit) operation, update the input matrix3d
 *
 * @param in        Floating point input matrix3d
 * @param clip      If value is higher than this, it will be clipped to this value
 */
void dl_matrix3d_relu_clip(dl_matrix3d_t *m, fptp_t clip);

/**
 * @brief Do a Prelu (Rectifier Linear Unit) operation, update the input matrix3d
 *
 * @param in        Floating point input matrix3d
 * @param alpha     If value is less than zero, it will be updated by multiplying this factor
 */
void dl_matrix3d_p_relu(dl_matrix3d_t *in, dl_matrix3d_t *alpha);

/**
 * @brief Do a leaky relu (Rectifier Linear Unit) operation, update the input matrix3d
 *
 * @param in        Floating point input matrix3d
 * @param alpha     If value is less than zero, it will be updated by multiplying this factor
 */
void dl_matrix3d_leaky_relu(dl_matrix3d_t *m, fptp_t alpha);

//
// Conv 1x1
//
void dl_matrix3dff_conv_1x1(dl_matrix3d_t *out,
                            dl_matrix3d_t *in,
                            dl_matrix3d_t *filter);

void dl_matrix3dff_conv_1x1_with_bias(dl_matrix3d_t *out,
                                      dl_matrix3d_t *in,
                                      dl_matrix3d_t *filter,
                                      dl_matrix3d_t *bias);

void dl_matrix3duf_conv_1x1(dl_matrix3d_t *out,
                            dl_matrix3du_t *in,
                            dl_matrix3d_t *filter);

void dl_matrix3duf_conv_1x1_with_bias(dl_matrix3d_t *out,
                                      dl_matrix3du_t *in,
                                      dl_matrix3d_t *filter,
                                      dl_matrix3d_t *bias);

//
// Conv 3x3
//
void dl_matrix3dff_conv_3x3_op(dl_matrix3d_t *out,
                               dl_matrix3d_t *in,
                               dl_matrix3d_t *f,
                               int step_x,
                               int step_y);

dl_matrix3d_t *dl_matrix3dff_conv_3x3(dl_matrix3d_t *in,
                                      dl_matrix3d_t *filter,
                                      dl_matrix3d_t *bias,
                                      int stride_x,
                                      int stride_y,
                                      dl_padding_type padding);

//
// Conv Common
//

dl_matrix3d_t *dl_matrix3duf_conv_common(dl_matrix3du_t *in,
                                         dl_matrix3d_t *filter,
                                         dl_matrix3d_t *bias,
                                         int stride_x,
                                         int stride_y,
                                         dl_padding_type padding);

//
// Depthwise 3x3
//

dl_matrix3d_t *dl_matrix3dff_depthwise_conv_3x3(dl_matrix3d_t *in,
                                                dl_matrix3d_t *filter,
                                                int stride_x,
                                                int stride_y,
                                                int padding);

dl_matrix3d_t *dl_matrix3duf_depthwise_conv_3x3(dl_matrix3du_t *in,
                                                dl_matrix3d_t *filter,
                                                int stride_x,
                                                int stride_y,
                                                int padding);

void dl_matrix3dff_depthwise_conv_3x3_op(dl_matrix3d_t *out,
                                         dl_matrix3d_t *in,
                                         dl_matrix3d_t *f,
                                         int step_x,
                                         int step_y);

//
// Depthwise Common
//

/**
 * @brief Do a depthwise CNN layer pass, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3d
 * @param filter         Weights of the neurons
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param padding        One of VALID or SAME
 * @param mode           Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                       If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return               The result of depthwise CNN layer
 */
dl_matrix3d_t *dl_matrix3dff_depthwise_conv_common(dl_matrix3d_t *in,
                                                   dl_matrix3d_t *filter,
                                                   int stride_x,
                                                   int stride_y,
                                                   dl_padding_type padding);

//
// FC
//
/**
 * @brief Do a general fully connected layer pass, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3d, size is (1, w, 1, 1)
 * @param filter         Weights of the neurons, size is (1, w, h, 1)
 * @param bias           Bias for the fc layer, size is (1, 1, 1, h)
 * @return               The result of fc layer, size is (1, 1, 1, h)
 */
void dl_matrix3dff_fc(dl_matrix3d_t *out,
                      dl_matrix3d_t *in,
                      dl_matrix3d_t *filter);

void dl_matrix3dff_fc_with_bias(dl_matrix3d_t *out,
                                dl_matrix3d_t *in,
                                dl_matrix3d_t *filter,
                                dl_matrix3d_t *bias);

//
// Mobilenet
//

/**
 * @brief Do a mobilenet block forward, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3d
 * @param filter         Weights of the neurons
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param padding        One of VALID or SAME
 * @param mode           Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                       If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return               The result of depthwise CNN layer
 */
dl_matrix3d_t *dl_matrix3dff_mobilenet(dl_matrix3d_t *in,
                                       dl_matrix3d_t *dilate_filter,
                                       dl_matrix3d_t *dilate_prelu,
                                       dl_matrix3d_t *depthwise_filter,
                                       dl_matrix3d_t *depthwise_prelu,
                                       dl_matrix3d_t *compress_filter,
                                       dl_matrix3d_t *bias,
                                       dl_matrix3d_mobilenet_config_t config);

/**
 * @brief Do a mobilenet block forward, dimension is (number, width, height, channel)
 *
 * @param in             Input matrix3du
 * @param filter         Weights of the neurons
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param padding        One of VALID or SAME
 * @param mode           Do convolution using C implement or xtensa implement, 0 or 1, with respect
 *                       If ESP_PLATFORM is not defined, this value is not used. Default is 0
 * @return               The result of depthwise CNN layer
 */
dl_matrix3d_t *dl_matrix3duf_mobilenet(dl_matrix3du_t *in,
                                       dl_matrix3d_t *dilate_filter,
                                       dl_matrix3d_t *dilate_prelu,
                                       dl_matrix3d_t *depthwise_filter,
                                       dl_matrix3d_t *depthwise_prelu,
                                       dl_matrix3d_t *compress_filter,
                                       dl_matrix3d_t *bias,
                                       dl_matrix3d_mobilenet_config_t config);
