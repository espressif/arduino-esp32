// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef DL_LIB_H
#define DL_LIB_H

#include "dl_lib_matrix.h"
#include "dl_lib_matrixq.h"
#include "dl_lib_matrixq8.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#define DL_SPIRAM_SUPPORT 1
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/cache.h"
#endif

typedef int padding_state;

// /**
//  * @brief Allocate a chunk of memory which has the given capabilities.
//  *        Equivalent semantics to libc malloc(), for capability-aware memory.
//  *        In IDF, malloc(p) is equivalent to heap_caps_malloc(p, MALLOC_CAP_8BIT).
//  * 
//  * @param size  In bytes, of the amount of memory to allocate
//  * @param caps  Bitwise OR of MALLOC_CAP_* flags indicating the type of memory to be returned
//  *              MALLOC_CAP_SPIRAM:   Memory must be in SPI RAM
//  *              MALLOC_CAP_INTERNAL: Memory must be internal; specifically it should not disappear when flash/spiram cache is switched off
//  *              MALLOC_CAP_DMA:      Memory must be able to accessed by DMA
//  *              MALLOC_CAP_DEFAULT:  Memory can be returned in a non-capability-specific memory allocation
//  * @return Pointer to currently allocated heap memory
//  **/
// void *heap_caps_malloc(size_t size, uint32_t caps);

/**
 * @brief Allocate aligned memory from internal memory or external memory.
 *        if cnt*size > CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL, allocate memory from internal RAM
 *        else, allocate memory from PSRAM
 *
 * @param cnt    Number of continuing chunks of memory to allocate
 * @param size   Size, in bytes, of a chunk of memory to allocate     
 * @param align  Aligned size, in bits
 * @return Pointer to currently allocated heap memory
 */
void *dl_lib_calloc(int cnt, int size, int align);

/**
 * @brief Always allocate aligned memory from external memory.
 *
 * @param cnt    Number of continuing chunks of memory to allocate
 * @param size   Size, in bytes, of a chunk of memory to allocate     
 * @param align  Aligned size, in bits
 * @return Pointer to currently aligned heap memory
 */
void *dl_lib_calloc_psram(int cnt, int size, int align);

/**
 * @brief Free aligned memory allocated by `dl_lib_calloc` or `dl_lib_calloc_psram` 
 * 
 * @param prt    Pointer to free
 */
void dl_lib_free(void *ptr);

/**
 * @brief Does a fast version of the exp() operation on a floating point number.
 *
 * As described in https://codingforspeed.com/using-faster-exponential-approximation/
 * Should be good til an input of 5 or so with a steps factor of 8.
 *
 * @param in Floating point input
 * @param steps Approximation steps. More is more precise. 8 or 10 should be good enough for most purposes.
 * @return Exp()'ed output
 */
fptp_t fast_exp(double x, int steps);

/**
 * @brief Does a fast version of the exp() operation on a floating point number.
 *
 * @param in Floating point input
 * @return Exp()'ed output
 */
double fast_exp_pro(double x);

/**
 * @brief Does a softmax operation on a matrix.
 *
 * @param in        Input matrix
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_softmax(const dl_matrix2d_t *in, dl_matrix2d_t *out);


/**
 * @brief Does a softmax operation on a quantized matrix.
 *
 * @param in        Input matrix
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_softmax_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);

/**
 * @brief Does a sigmoid operation on a floating point number
 *
 * @param in Floating point input
 * @return Sigmoid output
 */

fptp_t dl_sigmoid_op(fptp_t in);


/**
 * @brief Does a sigmoid operation on a matrix.
 *
 * @param in        Input matrix
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_sigmoid(const dl_matrix2d_t *in, dl_matrix2d_t *out);

/**
 * @brief Does a tanh operation on a floating point number
 *
 * @param in        Floating point input number
 * @return Tanh value
 */
fptp_t dl_tanh_op(fptp_t v);

/**
 * @brief Does a tanh operation on a matrix.
 *
 * @param in        Input matrix
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_tanh(const dl_matrix2d_t *in, dl_matrix2d_t *out);


/**
 * @brief Does a relu (Rectifier Linear Unit) operation on a floating point number
 *
 * @param in        Floating point input
 * @param clip      If value is higher than this, it will be clipped to this value
 * @return Relu output
 */
fptp_t dl_relu_op(fptp_t in, fptp_t clip);

/**
 * @brief Does a ReLu operation on a matrix.
 *
 * @param in        Input matrix
 * @param clip      If values are higher than this, they will be clipped to this value
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_relu(const dl_matrix2d_t *in, fptp_t clip, dl_matrix2d_t *out);

/**
 * @brief Fully connected layer operation
 *
 * @param in        Input vector
 * @param weight    Weights of the neurons
 * @param bias      Biases for the neurons. Can be NULL if a bias of 0 is required.
 * @param out       Output array. Outputs are placed here. Needs to be an initialized, weight->w by in->h in size, matrix.
 */
void dl_fully_connect_layer(const dl_matrix2d_t *in, const dl_matrix2d_t *weight, const dl_matrix2d_t *bias, dl_matrix2d_t *out);

/**
 * @brief Pre-calculate the sqrtvari variable for the batch_normalize function.
 * The sqrtvari matrix depends on the variance and epsilon values, which normally are constant. Hence,
 * this matrix only needs to be calculated once. This function does that.
 *
 * @param 
 * @return
 */
void dl_batch_normalize_get_sqrtvar(const dl_matrix2d_t *variance, fptp_t epsilon, dl_matrix2d_t *out);

/**
 * @brief Batch-normalize a matrix
 *
 * @param m         The matrix to normalize
 * @param offset    Offset matrix
 * @param scale     Scale matrix
 * @param mean      Mean matrix
 * @param sqrtvari  Matrix precalculated using dl_batch_normalize_get_sqrtvar
 * @return
 */
void dl_batch_normalize(dl_matrix2d_t *m, const dl_matrix2d_t *offset, const dl_matrix2d_t *scale, 
                        const dl_matrix2d_t *mean, const dl_matrix2d_t *sqrtvari);

/**
 * @brief Do a basic LSTM layer pass.
 *
 * @warning Returns state_h pointer, so do not free result.

 * @param in        Input vector
 * @param state_c   Internal state of the LSTM network
 * @param state_h   Internal state (previous output values) of the LSTM network
 * @param weights   Weights for the neurons
 * @param bias      Bias for the neurons. Can be NULL if no bias is required
 * @return          Output values of the neurons
 */
dl_matrix2d_t *dl_basic_lstm_layer(const dl_matrix2d_t *in, dl_matrix2d_t *state_c, dl_matrix2d_t *state_h, 
                const dl_matrix2d_t *weight, const dl_matrix2d_t *bias);

/**
 * @brief Do a basic LSTM layer pass, partial quantized version.
 * This LSTM function accepts 16-bit fixed-point weights and 32-bit float-point bias. 
 *
 * @warning Returns state_h pointer, so do not free result.

 * @param in		Input vector
 * @param state_c	Internal state of the LSTM network
 * @param state_h	Internal state (previous output values) of the LSTM network
 * @param weights	Weights for the neurons, need to be quantised 
 * @param bias		Bias for the neurons. Can be NULL if no bias is required
 * @return			Output values of the neurons
 */
dl_matrix2dq_t *dl_basic_lstm_layer_quantised_weights(const dl_matrix2d_t *in, dl_matrix2d_t *state_c, dl_matrix2d_t *state_h,
				const dl_matrix2dq_t *weight, const dl_matrix2d_t *bias);

/**
 * @brief Do a fully-connected layer pass, fully-quantized version.
 *
 * @param in        Input vector
 * @param weight    Weights of the neurons
 * @param bias      Bias values of the neurons. Can be NULL if no bias is needed.
 * @param shift     Number of bits to shift the result back by. See dl_lib_matrixq.h for more info
 * @return          Output values of the neurons
 */
void dl_fully_connect_layer_q(const dl_matrix2dq_t *in, const dl_matrix2dq_t *weight, const dl_matrix2dq_t *bias, dl_matrix2dq_t *out, int shift);

/**
 * @brief Do a basic LSTM layer pass, fully-quantized version
 *
 * @warning Returns state_h pointer, so do not free result.

 * @param in        Input vector
 * @param state_c   Internal state of the LSTM network
 * @param state_h   Internal state (previous output values) of the LSTM network
 * @param weights   Weights for the neurons
 * @param bias      Bias for the neurons. Can be NULL if no bias is required
 * @param shift     Number of bits to shift the result back by. See dl_lib_matrixq.h for more info
 * @return          Output values of the neurons
 */
dl_matrix2dq_t *dl_basic_lstm_layer_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *state_c, dl_matrix2dq_t *state_h,
                const dl_matrix2dq_t *weight, const dl_matrix2dq_t *bias, int shift);

/**
 * @brief Batch-normalize a matrix, fully-quantized version
 *
 * @param m         The matrix to normalize
 * @param offset    Offset matrix
 * @param scale     Scale matrix
 * @param mean      Mean matrix
 * @param sqrtvari  Matrix precalculated using dl_batch_normalize_get_sqrtvar
 * @param shift     Number of bits to shift the result back by. See dl_lib_matrixq.h for more info
 * @return
 */
void dl_batch_normalize_q(dl_matrix2dq_t *m, const dl_matrix2dq_t *offset, const dl_matrix2dq_t *scale, 
                        const dl_matrix2dq_t *mean, const dl_matrix2dq_t *sqrtvari, int shift);

/**
 * @brief Does a relu (Rectifier Linear Unit) operation on a fixed-point number
 * This accepts and returns fixed-point 32-bit number with the last 15 bits being the bits after the decimal
 * point. (Equivalent to a mantissa in a quantized matrix with exponent -15.)
 *
 * @param in        Fixed-point input
 * @param clip      If value is higher than this, it will be clipped to this value
 * @return Relu output
 */
qtp_t dl_relu_q_op(qtp_t in, qtp_t clip);

/**
 * @brief Does a ReLu operation on a matrix, quantized version
 *
 * @param in        Input matrix
 * @param clip      If values are higher than this, they will be clipped to this value
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_relu_q(const dl_matrix2dq_t *in, fptp_t clip, dl_matrix2dq_t *out);

/**
 * @brief Does a sigmoid operation on a fixed-point number.
 * This accepts and returns a fixed-point 32-bit number with the last 15 bits being the bits after the decimal
 * point. (Equivalent to a mantissa in a quantized matrix with exponent -15.)
 *
 * @param in Fixed-point input
 * @return Sigmoid output
 */
int dl_sigmoid_op_q(const int in);
int16_t dl_sigmoid_op_q8(const int16_t in);
/**
 * @brief Does a sigmoid operation on a matrix, quantized version
 *
 * @param in        Input matrix
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_sigmoid_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);

/**
 * @brief Does a tanh operation on a matrix, quantized version
 *
 * @param in        Input matrix
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_tanh_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);

/**
 * @brief Does a tanh operation on a fixed-point number.
 * This accepts and returns a fixed-point 32-bit number with the last 15 bits being the bits after the decimal
 * point. (Equivalent to a mantissa in a quantized matrix with exponent -15.)
 *
 * @param in Fixed-point input
 * @return tanh output
 */
int dl_tanh_op_q(int v);
int16_t dl_tanh_op_q8(int16_t v);

void load_mat_psram_mn4(void);
void load_mat_psram_mn3(void);
void free_mat_psram_mn4(void);
void free_mat_psram_mn3(void);
qtp_t dl_hard_sigmoid_op(qtp_t in, int exponent);
qtp_t dl_hard_tanh_op(qtp_t in, int exponent);

int16_t dl_table_tanh_op(int16_t in, int exponent);
int16_t dl_table_sigmoid_op(int16_t in, int exponent);

void dl_hard_sigmoid_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);
void dl_hard_tanh_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);

void dl_table_sigmoid_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);
void dl_table_tanh_q(const dl_matrix2dq_t *in, dl_matrix2dq_t *out);


/**
 * @brief Filter out the number greater than clip in the matrix, quantized version
 *
 * @param in        Input matrix
 * @param clip      If values are higher than this, they will be clipped to this value
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_minimum(const dl_matrix2d_t *in, fptp_t clip, dl_matrix2d_t *out);

/**
 * @brief Filter out the number greater than clip in the matrix, float version
 *
 * @param in        Input matrix
 * @param clip      If values are higher than this, they will be clipped to this value
 * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
 */
void dl_minimum_q(const dl_matrix2dq_t *in, fptp_t clip, dl_matrix2dq_t *out);
/**
 * @brief Do a basic CNN layer pass.
 *
 * @Warning This just supports the single channel input image, and the output is single row matrix.
            That is to say, the height of output is 1, and the weight of output is out_channels*out_image_width*out_image_height
 *
 * @param in             Input single channel image 
 * @param weight         Weights of the neurons, weight->w = out_channels, weight->h = filter_width*filter_height
 * @param bias           Bias for the CNN layer.
 * @param filter_height  The height of convolution kernel
 * @param filter_width   The width of convolution kernel
 * @param out_channels   The number of output channels of convolution kernel
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param pad            One of `"VALID"` or `"SAME"`, 0 is "VALID" and the other is "SAME"
 * @param out            The result of CNN layer, out->h=1.
 * @return               The result of CNN layer.
 */
dl_matrix2d_t *dl_basic_conv_layer(const dl_matrix2d_t *in, const dl_matrix2d_t *weight, const dl_matrix2d_t *bias, int filter_width, int filter_height, 
                                   const int out_channels, const int stride_x, const int stride_y,  padding_state pad, const dl_matrix2d_t* out);


/**
 * @brief Do a basic CNN layer pass, quantised wersion.
 *
 * @Warning This just supports the single channel input image, and the output is single row matrix.
            That is to say, the height of output is 1, and the weight of output is out_channels*out_image_width*out_image_height
 *
 * @param in             Input single channel image 
 * @param weight         Weights of the neurons, weight->w = out_channels, weight->h = filter_width*filter_height,
 * @param bias           Bias of the neurons.
 * @param filter_height  The height of convolution kernel
 * @param filter_width   The width of convolution kernel
 * @param out_channels   The number of output channels of convolution kernel
 * @param stride_x       The step length of the convolution window in x(width) direction
 * @param stride_y       The step length of the convolution window in y(height) direction
 * @param pad            One of `"VALID"` or `"SAME"`, 0 is "VALID" and the other is "SAME"
 * @param out            The result of CNN layer, out->h=1
 * @return               The result of CNN layer
 */
dl_matrix2d_t *dl_basic_conv_layer_quantised_weight(const dl_matrix2d_t *in, const dl_matrix2dq_t *weight, const dl_matrix2d_t *bias, int filter_width, int filter_height, 
                                                     const int out_channels, const int stride_x, const int stride_y,  padding_state pad, const dl_matrix2d_t* out);

#endif

