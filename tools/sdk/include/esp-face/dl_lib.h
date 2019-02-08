#ifndef DL_LIB_H
#define DL_LIB_H
#ifdef __cplusplus
extern "C" {
#endif

#include "dl_lib_matrix.h"
#include "dl_lib_matrixq.h"
#include "dl_lib_matrix3d.h"
#include "dl_lib_matrix3dq.h"

    typedef int padding_state;
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
     * @brief Does a softmax operation on a matrix.
     *
     * @param in        Input matrix
     * @param out       Output matrix. Can be the same as the input matrix; if so,
                                             output results overwrite the input.
    */
    void dl_softmax(const dl_matrix2d_t *in,
                    dl_matrix2d_t *out);

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
    void dl_fully_connect_layer(const dl_matrix2d_t *in,
                                const dl_matrix2d_t *weight,
                                const dl_matrix2d_t *bias,
                                dl_matrix2d_t *out);

    /**
     * @brief Pre-calculate the sqrtvari variable for the batch_normalize function.
     * The sqrtvari matrix depends on the variance and epsilon values, which normally are constant. Hence,
     * this matrix only needs to be calculated once. This function does that.
     *
     * @param
     * @return
     */
    void dl_batch_normalize_get_sqrtvar(const dl_matrix2d_t *variance,
                                        fptp_t epsilon,
                                        dl_matrix2d_t *out);

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
    void dl_batch_normalize(dl_matrix2d_t *m,
                            const dl_matrix2d_t *offset,
                            const dl_matrix2d_t *scale,
                            const dl_matrix2d_t *mean,
                            const dl_matrix2d_t *sqrtvari);

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
    dl_matrix2d_t *dl_basic_lstm_layer(const dl_matrix2d_t *in,
                                       dl_matrix2d_t *state_c,
                                       dl_matrix2d_t *state_h,
                                       const dl_matrix2d_t *weight,
                                       const dl_matrix2d_t *bias);

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
    dl_matrix2d_t *dl_basic_lstm_layer_quantised_weights(const dl_matrix2d_t *in,
                                                         dl_matrix2d_t *state_c,
                                                         dl_matrix2d_t *state_h,
                                                         const dl_matrix2dq_t *weight,
                                                         const dl_matrix2d_t *bias);

    /**
     * @brief Do a fully-connected layer pass, fully-quantized version.
     *
     * @param in        Input vector
     * @param weight    Weights of the neurons
     * @param bias      Bias values of the neurons. Can be NULL if no bias is needed.
     * @param shift     Number of bits to shift the result back by. See dl_lib_matrixq.h for more info
     * @return          Output values of the neurons
     */
    void dl_fully_connect_layer_q(const dl_matrix2dq_t *in,
                                  const dl_matrix2dq_t *weight,
                                  const dl_matrix2dq_t *bias,
                                  dl_matrix2dq_t *out,
                                  int shift);

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
    dl_matrix2dq_t *dl_basic_lstm_layer_q(const dl_matrix2dq_t *in,
                                          dl_matrix2dq_t *state_c,
                                          dl_matrix2dq_t *state_h,
                                          const dl_matrix2dq_t *weight,
                                          const dl_matrix2dq_t *bias,
                                          int shift);

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
    void dl_batch_normalize_q(dl_matrix2dq_t *m,
                              const dl_matrix2dq_t *offset,
                              const dl_matrix2dq_t *scale,
                              const dl_matrix2dq_t *mean,
                              const dl_matrix2dq_t *sqrtvari,
                              int shift);

    /**
     * @brief Does a relu (Rectifier Linear Unit) operation on a fixed-point number
     * This accepts and returns fixed-point 32-bit number with the last 15 bits being the bits after the decimal
     * point. (Equivalent to a mantissa in a quantized matrix with exponent -15.)
     *
     * @param in        Fixed-point input
     * @param clip      If value is higher than this, it will be clipped to this value
     * @return Relu output
     */
    qtp_t dl_relu_q_op(qtp_t in,
                       qtp_t clip);

    /**
     * @brief Does a ReLu operation on a matrix, quantized version
     *
     * @param in        Input matrix
     * @param clip      If values are higher than this, they will be clipped to this value
     * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
     */
    void dl_relu_q(const dl_matrix2dq_t *in,
                   fptp_t clip,
                   dl_matrix2dq_t *out);

    /**
     * @brief Does a sigmoid operation on a fixed-point number.
     * This accepts and returns a fixed-point 32-bit number with the last 15 bits being the bits after the decimal
     * point. (Equivalent to a mantissa in a quantized matrix with exponent -15.)
     *
     * @param in Fixed-point input
     * @return Sigmoid output
     */
    int dl_sigmoid_op_q(const int in);

    /**
     * @brief Does a sigmoid operation on a matrix, quantized version
     *
     * @param in        Input matrix
     * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
     */
    void dl_sigmoid_q(const dl_matrix2dq_t *in,
                      dl_matrix2dq_t *out);

    /**
     * @brief Does a tanh operation on a matrix, quantized version
     *
     * @param in        Input matrix
     * @param out       Output matrix. Can be the same as the input matrix; if so, output results overwrite the input.
     */
    void dl_tanh_q(const dl_matrix2dq_t *in,
                   dl_matrix2dq_t *out);

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
    dl_matrix2d_t *dl_basic_conv_layer(const dl_matrix2d_t *in,
                                       const dl_matrix2d_t *weight,
                                       const dl_matrix2d_t *bias,
                                       int filter_width,
                                       int filter_height,
                                       const int out_channels,
                                       const int stride_x,
                                       const int stride_y,
                                       padding_state pad,
                                       const dl_matrix2d_t *out);

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
    dl_matrix2d_t *dl_basic_conv_layer_quantised_weight(const dl_matrix2d_t *in,
                                                        const dl_matrix2dq_t *weight,
                                                        const dl_matrix2d_t *bias,
                                                        int filter_width,
                                                        int filter_height,
                                                        const int out_channels,
                                                        const int stride_x,
                                                        const int stride_y,
                                                        padding_state pad,
                                                        const dl_matrix2d_t *out);

#ifdef __cplusplus
}
#endif
#endif
