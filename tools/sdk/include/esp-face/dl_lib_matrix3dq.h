#pragma once
#include "dl_lib_matrix3d.h"

typedef int16_t qtp_t;

/**
 * Matrix for input, filter, and output
 * @Warning: the sequence of variables is fixed, cannot be modified, otherwise there will be errors in 
 * some handwrite xtensa instruction functions
 */
typedef struct
{
    /******* fix start *******/
    int w;          /*!< Width */
    int h;          /*!< Height */
    int c;          /*!< Channel */
    int n;          /*!< Number of filter, input and output must be 1 */
    int stride;     /*!< Step between lines */
    int exponent;   /*!< Exponent for quantization */
    qtp_t *item;    /*!< Data */
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

/**
 * Implementation of matrix relative operations
 */
typedef enum
{
    DL_C_IMPL = 0,      /*!< ANSI C */
    DL_XTENSA_IMPL = 1  /*!< Handwrite xtensa instruction */
} dl_conv_mode;


/**
 * Configuration of mobilenet operation
 */
typedef struct
{
    int stride_x;               /*!< Strides of width */
    int stride_y;               /*!< Strides of height */
    dl_padding_type padding;    /*!< Padding type */
    dl_conv_mode mode;          /*!< Implementation mode */
    int dilate_exponent;        /*!< Exponent of dilation filter */
    int depthwise_exponent;     /*!< Exponent of depthwise filter */
    int compress_exponent;      /*!< Exponent of compress filter */
} dl_matrix3dq_mobilenet_config_t;

//
// Utility
//

/*
 * @brief Allocate a 3d quantised matrix
 *
 * @param n Number of filters, for input and output, should be 1
 * @param w Width of matrix
 * @param h Height of matrix
 * @param c Channel of matrix
 * @param e Exponent of matrix data
 * @return 3d quantized matrix
 */
dl_matrix3dq_t *dl_matrix3dq_alloc(int n, int w, int h, int c, int e);

/*
 * @brief Free a 3d quantized matrix
 *
 * @param m 3d quantised matrix
 */
void dl_matrix3dq_free(dl_matrix3dq_t *m);

/**
 * @brief Copy a range of items from an existing matrix to a preallocated matrix
 *
 * @param dst   The resulting slice matrix
 * @param src   Old matrix to slice.
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 */
void dl_matrix3dq_slice_copy(dl_matrix3dq_t *dst, dl_matrix3dq_t *src, int x, int y, int w, int h);

/**
 * @brief Transform a fixed point matrix to a float point matrix
 *
 * @param m     Quantized matrix
 * @return      Float point matrix
 */
dl_matrix3d_t *dl_matrix3d_from_matrixq(dl_matrix3dq_t *m);

/**
 * @brief Transform a float point matrix to a fixed point matrix with pre-defined exponent
 *
 * @param m         Float point matrix
 * @param exponent  Exponent for resulting matrix
 * @return          Fixed point matrix
 */
dl_matrix3dq_t *dl_matrixq_from_matrix3d_qmf(dl_matrix3d_t *m, int exponent);

/**
 * @brief Transform a float point matrix to a fixed point matrix. The exponent is defined by the distribution of the input matrix.
 *
 * @param m         Float point matrix
 * @return          Fixed point matrix
 */
dl_matrix3dq_t *dl_matrixq_from_matrix3d(dl_matrix3d_t *m);

qtp_t dl_matrix3dq_quant_range_exceeded_checking(int64_t value, char *location);

/**
 * @brief Reform a quantized matrix with exponent
 *
 * @param out       Preallocated resulting matrix
 * @param in        Input matrix
 * @param exponent  Exponent for resulting matrix
 */
void dl_matrix3dq_shift_exponent(dl_matrix3dq_t *out, dl_matrix3dq_t *in, int exponent);

/**
 * @brief Do batch normalization for a quantized matrix
 *
 * @param m         Input and output quantized matrix, data will be updated
 * @param scale     Scale of batch-norm
 * @param offset    Offset of batch-norm
 */
void dl_matrix3dq_batch_normalize(dl_matrix3dq_t *m, dl_matrix3dq_t *scale, dl_matrix3dq_t *offset);

/**
 * @brief Add two quantized matrix with a pre-defined exponent
 *
 * @param in_1      Adder 1
 * @param in_2      Adder 2
 * @param exponent  Exponent for resulting matrix
 * @return          Result of accumulation of two matrix
 */
dl_matrix3dq_t *dl_matrix3dq_add(dl_matrix3dq_t *in_1, dl_matrix3dq_t *in_2, int exponent);

//
// Activation
//
/**
 * @brief Do relu for a quantized matrix
 *
 * @param in        Input and output quantized matrix, data will be updated
 */
void dl_matrix3dq_relu(dl_matrix3dq_t *in);

/**
 * @brief Do relu with clips for a quantized matrix
 *
 * @param in        Input and output quantized matrix, data will be updated
 * @param clip      Float point value to limit the maximum data
 */
void dl_matrix3dq_relu_clip(dl_matrix3dq_t *in, fptp_t clip);

/**
 * @brief Do leaky relu for a quantized matrix
 *
 * @param in        Input and output quantized matrix, data will be updated
 * @param alpha     Float point value to multiply for those less than zero
 * @param clip      Float point value to limit the maximum data
 */
void dl_matrix3dq_leaky_relu(dl_matrix3dq_t *in, fptp_t alpha, fptp_t clip);

/**
 * @brief Do prelu for a quantized matrix
 *
 * @param in        Input and output quantized matrix, data will be updated
 * @param alpha     Quantized matrix to multiply for those less than zero
 */
void dl_matrix3dq_p_relu(dl_matrix3dq_t *in, dl_matrix3dq_t *alpha);

//
// Concat
//
/**
 * @brief Concatenate two quantized matrix in channel
 *
 * @param in_1      Quantized matrix to be concatenated
 * @param in_2      Quantized matrix to be concatenated
 * @return          Quantized matrix with the same width and height of in_1 and in_2, and with the sum of channel number of in_1 and in_2
 */
dl_matrix3dq_t *dl_matrix3dq_concat(dl_matrix3dq_t *in_1,
                                    dl_matrix3dq_t *in_2);

/**
 * @brief Concatenate four quantized matrix in channel
 *
 * @param in_1      Quantized matrix to be concatenated
 * @param in_2      Quantized matrix to be concatenated
 * @param in_3      Quantized matrix to be concatenated
 * @param in_4      Quantized matrix to be concatenated
 * @return          Quantized matrix with the same width and height of all inputs, and with the sum of channel number of all inputs
 */
dl_matrix3dq_t *dl_matrix3dq_concat_4(dl_matrix3dq_t *in_1,
                                      dl_matrix3dq_t *in_2,
                                      dl_matrix3dq_t *in_3,
                                      dl_matrix3dq_t *in_4);

/**
 * @brief Concatenate four quantized matrix in channel
 *
 * @param in_1      Quantized matrix to be concatenated
 * @param in_2      Quantized matrix to be concatenated
 * @param in_3      Quantized matrix to be concatenated
 * @param in_4      Quantized matrix to be concatenated
 * @param in_5      Quantized matrix to be concatenated
 * @param in_6      Quantized matrix to be concatenated
 * @param in_7      Quantized matrix to be concatenated
 * @param in_8      Quantized matrix to be concatenated
 * @return          Quantized matrix with the same width and height of all inputs, and with the sum of channel number of all inputs
 */
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
/**
 * @brief Do 1x1 convolution with a quantized matrix
 *
 * @param out       Preallocated quantized matrix, size (1, w, h, n)
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    1x1 filter, size (n, 1, 1, c)
 * @param mode      Implementation mode
 */
void dl_matrix3dqq_conv_1x1(dl_matrix3dq_t *out,
                            dl_matrix3dq_t *in,
                            dl_matrix3dq_t *filter,
                            dl_conv_mode mode);

/**
 * @brief Do 1x1 convolution with a quantized matrix, with relu activation
 *
 * @param out       Preallocated quantized matrix, size (1, w, h, n)
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    1x1 filter, size (n, 1, 1, c)
 * @param mode      Implementation mode
 */
void dl_matrix3dqq_conv_1x1_with_relu(dl_matrix3dq_t *out,
                                      dl_matrix3dq_t *in,
                                      dl_matrix3dq_t *filter,
                                      dl_conv_mode mode);

/**
 * @brief Do 1x1 convolution with a quantized matrix, with bias adding
 *
 * @param out       Preallocated quantized matrix, size (1, w, h, n)
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    1x1 filter, size (n, 1, 1, c)
 * @param bias      Bias, size (1, 1, 1, n)
 * @param mode      Implementation mode
 * @param name      Layer name to debug
 */
void dl_matrix3dqq_conv_1x1_with_bias(dl_matrix3dq_t *out,
                                      dl_matrix3dq_t *in,
                                      dl_matrix3dq_t *filter,
                                      dl_matrix3dq_t *bias,
                                      dl_conv_mode mode,
                                      char *name);

/**
 * @brief Do 1x1 convolution with a quantized matrix, with bias adding and relu activation
 *
 * @param out       Preallocated quantized matrix, size (1, w, h, n)
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    1x1 filter, size (n, 1, 1, c)
 * @param bias      Bias, size (1, 1, 1, n)
 * @param mode      Implementation mode
 */
void dl_matrix3dqq_conv_1x1_with_bias_relu(dl_matrix3dq_t *out,
                                           dl_matrix3dq_t *in,
                                           dl_matrix3dq_t *filter,
                                           dl_matrix3dq_t *bias,
                                           dl_conv_mode mode);

void dl_matrix3dqq_conv_1x1_with_prelu(dl_matrix3dq_t *out,
                                       dl_matrix3dq_t *in,
                                       dl_matrix3dq_t *filter,
                                       dl_matrix3dq_t *prelu,
                                       dl_conv_mode mode);

/**
 * @brief Do 1x1 convolution with an 8-bit fixed point matrix
 *
 * @param out       Preallocated quantized matrix, size (1, w, h, n)
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    1x1 filter, size (n, 1, 1, c)
 * @param mode      Implementation mode
 */
void dl_matrix3duq_conv_1x1(dl_matrix3dq_t *out,
                            dl_matrix3du_t *in,
                            dl_matrix3dq_t *filter,
                            dl_conv_mode mode);

/**
 * @brief Do 1x1 convolution with an 8-bit fixed point matrix, with bias adding
 *
 * @param out       Preallocated quantized matrix, size (1, w, h, n)
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    1x1 filter, size (n, 1, 1, c)
 * @param bias      Bias, size (1, 1, 1, n)
 * @param mode      Implementation mode
 */
void dl_matrix3duq_conv_1x1_with_bias(dl_matrix3dq_t *out,
                                      dl_matrix3du_t *in,
                                      dl_matrix3dq_t *filter,
                                      dl_matrix3dq_t *bias,
                                      dl_conv_mode mode);

//
// Conv 3x3
//
/**
 * @brief Do 3x3 convolution basic operation with a quantized matrix
 *
 * @param out       Preallocated quantized matrix
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (n, 3, 3, c)
 * @param stride_x  Stride of width
 * @param stride_y  Stride of height
 */
void dl_matrix3dqq_conv_3x3_op(dl_matrix3dq_t *out,
                               dl_matrix3dq_t *in,
                               dl_matrix3dq_t *filter,
                               int stride_x,
                               int stride_y);

/**
 * @brief Do 3x3 convolution with a quantized matrix
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (n, 3, 3, c)
 * @param stride_x  Stride of width
 * @param stride_y  Stride of height
 * @param padding   Padding type, 0: valid, 1: same
 * @param exponent  Exponent for resulting matrix
 * @return          Resulting quantized matrix
 */
dl_matrix3dq_t *dl_matrix3dqq_conv_3x3(dl_matrix3dq_t *in,
                                       dl_matrix3dq_t *filter,
                                       int stride_x,
                                       int stride_y,
                                       dl_padding_type padding,
                                       int exponent);

/**
 * @brief Do 3x3 convolution with a quantized matrix, with bias adding
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (n, 3, 3, c)
 * @param bias      Bias, size (1, 1, 1, n)
 * @param stride_x  Stride of width
 * @param stride_y  Stride of height
 * @param padding   Padding type, 0: valid, 1: same
 * @param exponent  Exponent for resulting matrix
 * @return          Resulting quantized matrix
 */
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
 * @brief Do a general convolution layer pass, size is (number, width, height, channel)
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

/**
 * @brief Do a general convolution layer pass for an 8-bit fixed point matrix, size is (number, width, height, channel)
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
/**
 * @brief Do 3x3 depthwise convolution with an 8-bit fixed point matrix
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (1, 3, 3, c)
 * @param stride_x  Stride of width
 * @param stride_y  Stride of height
 * @param padding   Padding type, 0: valid, 1: same
 * @param exponent  Exponent for resulting matrix
 * @return          Resulting quantized matrix
 */
dl_matrix3dq_t *dl_matrix3duq_depthwise_conv_3x3(dl_matrix3du_t *in,
                                                 dl_matrix3dq_t *filter,
                                                 int stride_x,
                                                 int stride_y,
                                                 dl_padding_type padding,
                                                 int exponent);

/**
 * @brief Do 3x3 depthwise convolution with a quantized matrix
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (1, 3, 3, c)
 * @param stride_x  Stride of width
 * @param stride_y  Stride of height
 * @param padding   Padding type, 0: valid, 1: same
 * @param exponent  Exponent for resulting matrix
 * @return          Resulting quantized matrix
 */
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

/**
 * @brief Do 3x3 depthwise convolution with a quantized matrix, with bias adding
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (1, 3, 3, c)
 * @param bias      Bias, size (1, 1, 1, c)
 * @param stride_x  Stride of width
 * @param stride_y  Stride of height
 * @param padding   Padding type, 0: valid, 1: same
 * @param exponent  Exponent for resulting matrix
 * @param relu      Whether to use relu activation
 * @return          Resulting quantized matrix
 */
dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3_with_bias(dl_matrix3dq_t *in,
                                                          dl_matrix3dq_t *f,
                                                          dl_matrix3dq_t *bias,
                                                          int stride_x,
                                                          int stride_y,
                                                          dl_padding_type padding,
                                                          int exponent,
                                                          int relu);

/**
 * @brief Do 3x3 depthwise convolution with a quantized matrix, with bias adding and stride 1
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @param filter    3x3 filter, size (1, 3, 3, c)
 * @param bias      Bias, size (1, 1, 1, n)
 * @param padding   Padding type, 0: valid, 1: same
 * @param exponent  Exponent for resulting matrix
 * @param relu      Whether to use relu activation
 * @return          Resulting quantized matrix
 */
dl_matrix3dq_t *dl_matrix3dqq_depthwise_conv_3x3s1_with_bias(dl_matrix3dq_t *in,
                                                            dl_matrix3dq_t *f,
                                                            dl_matrix3dq_t *bias,
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
/**
 * @brief Do fully connected layer forward.
 *
 * @param out       Preallocated resulting matrix, size (1, 1, 1, h)
 * @param in        Input matrix, size (1, 1, 1, w)
 * @param filter    Filter matrix, size (1, w, h, 1)
 * @param mode      Implementation mode
 */
void dl_matrix3dqq_fc(dl_matrix3dq_t *out,
                      dl_matrix3dq_t *in,
                      dl_matrix3dq_t *filter,
                      dl_conv_mode mode);

/**
 * @brief Do fully connected layer forward, with bias adding
 *
 * @param out       Preallocated resulting matrix, size (1, 1, 1, h)
 * @param in        Input matrix, size (1, 1, 1, w)
 * @param filter    Filter matrix, size (1, w, h, 1)
 * @param bias      Bias matrix, size (1, 1, 1, h)
 * @param mode      Implementation mode
 */
void dl_matrix3dqq_fc_with_bias(dl_matrix3dq_t *out,
                                dl_matrix3dq_t *in,
                                dl_matrix3dq_t *filter,
                                dl_matrix3dq_t *bias,
                                dl_conv_mode mode,
                                char *name);

//
// Mobilefaceblock
//
/**
 * @brief Do mobilefacenet process with splited pointwise 1x1 convolution, the process sequence is 1x1 pointwise->bn->relu->3x3 depthwise->bn->relu->1x1 pointwise->bn
 *
 * @param in                    Input matrix, size (1, w, h, c)
 * @param pw_1                  Pointwise 1x1 filter, size (n1/2, 1, 1, c)
 * @param pw_2                  Pointwise 1x1 filter, size (n1/2, 1, 1, c)
 * @param pw_bias               Pointwise bias, size (1, 1, 1, n1)
 * @param dw                    Depthwise 3x3 filter, size (1, 3, 3, n1)
 * @param dw_bias               Depthwise bias, size (1, 1, 1, n1)
 * @param pw_linear_1           Pointwise 1x1 filter, size (n2/2, 1, 1, n1)
 * @param pw_linear_2           Pointwise 1x1 filter, size (n2/2, 1, 1, n1)
 * @param pw_linear_bias        Pointwise bias, size (1, 1, 1, n2)
 * @param pw_exponent           Exponent for pointwise resulting matrix
 * @param dw_exponent           Exponent for depthwise resulting matrix
 * @param pw_linear_exponent    Exponent for pointwise resulting matrix
 * @param stride_x              Stride of width
 * @param stride_y              Stride of height
 * @param padding               Padding type, 0: valid, 1: same
 * @param mode                  Implementation mode
 * @param shortcut              Whether has a shortcut at pointwise linear
 * @return                      Resulting quantized matrix
 */
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

/**
 * @brief Do mobilefacenet process, the process sequence is 1x1 pointwise->bn->relu->3x3 depthwise->bn->relu->1x1 pointwise->bn
 *
 * @param in                    Input matrix, size (1, w, h, c)
 * @param pw                    Pointwise 1x1 filter, size (n1, 1, 1, c)
 * @param pw_bias               Pointwise bias, size (1, 1, 1, n1)
 * @param dw                    Depthwise 3x3 filter, size (1, 3, 3, n1)
 * @param dw_bias               Depthwise bias, size (1, 1, 1, n1)
 * @param pw_linear             Pointwise 1x1 filter, size (n2, 1, 1, n1)
 * @param pw_linear_bias        Pointwise bias, size (1, 1, 1, n2)
 * @param pw_exponent           Exponent for pointwise resulting matrix
 * @param dw_exponent           Exponent for depthwise resulting matrix
 * @param pw_linear_exponent    Exponent for pointwise resulting matrix
 * @param stride_x              Stride of width
 * @param stride_y              Stride of height
 * @param padding               Padding type, 0: valid, 1: same
 * @param mode                  Implementation mode
 * @param shortcut              Whether has a shortcut at pointwise linear
 * @return                      Resulting quantized matrix
 */
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

/**
 * @brief Do mobilenet process, the process sequence is 1x1 dilated->prelu->3x3 depthwise->prelu->1x1 compress->bias
 *
 * @param in                    Input matrix, size (1, w, h, c)
 * @param dilate                Pointwise 1x1 filter, size (n1, 1, 1, c)
 * @param dilate_prelu          Pointwise prelu, size (1, 1, 1, n1)
 * @param depthwise             Depthwise 3x3 filter, size (1, 3, 3, n1)
 * @param depthwise_prelu       Depthwise prelu, size (1, 1, 1, n1)
 * @param compress              Pointwise 1x1 filter, size (n2, 1, 1, n1)
 * @param bias                  Pointwise bias, size (1, 1, 1, n2)
 * @param config                Mobilenet configuration
 * @return                      Resulting quantized matrix
 */
dl_matrix3dq_t *dl_matrix3dqq_mobilenet(dl_matrix3dq_t *in,
                                        dl_matrix3dq_t *dilate,
                                        dl_matrix3dq_t *dilate_prelu,
                                        dl_matrix3dq_t *depthwise,
                                        dl_matrix3dq_t *depth_prelu,
                                        dl_matrix3dq_t *compress,
                                        dl_matrix3dq_t *bias,
                                        dl_matrix3dq_mobilenet_config_t config,
                                        char *name);

/**
 * @brief Do mobilenet process, the process sequence is 1x1 dilated->prelu->3x3 depthwise->prelu->1x1 compress->bias
 *
 * @param in                    Input matrix, 8-bit fixed point, size (1, w, h, c)
 * @param dilate                Pointwise 1x1 filter, size (n1, 1, 1, c)
 * @param dilate_prelu          Pointwise prelu, size (1, 1, 1, n1)
 * @param depthwise             Depthwise 3x3 filter, size (1, 3, 3, n1)
 * @param depthwise_prelu       Depthwise prelu, size (1, 1, 1, n1)
 * @param compress              Pointwise 1x1 filter, size (n2, 1, 1, n1)
 * @param bias                  Pointwise bias, size (1, 1, 1, n2)
 * @param config                Mobilenet configuration
 * @return                      Resulting quantized matrix
 */
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
/**
 * @brief Calculate average value of a feature map
 *
 * @param in        Input matrix, size (1, w, h, c)
 * @return          Resulting matrix, size (1, 1, 1, c)
 */
dl_matrix3dq_t *dl_matrix3dq_global_pool(dl_matrix3dq_t *in);
