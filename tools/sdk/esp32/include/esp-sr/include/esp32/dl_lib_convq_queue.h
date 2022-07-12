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
#ifndef DL_LIB_CONVQ_QUEUE_H
#define DL_LIB_CONVQ_QUEUE_H

#include "dl_lib_matrixq.h"
#include "dl_lib_conv_queue.h"
#include "dl_lib.h"


//fixed-point convolution FIFO queue. 
//[nch, n, c]
typedef struct {
    int n;           /*< the length of queue */
    int c;           /*< the number of queue element*/
    int front;       /*< the front(top) position of queue */
    int nch;         /*< the multiple of queue*/
    int exponent;    /*< The values in items should be multiplied by pow(2,exponent) 
                         to get the real values */
    qtp_t *itemq;    /*< Pointer to item array */
} dl_convq_queue_t;

/**
 * @brief Allocate a fixed-point convolution queue
 *
 * @param n     The length of queue
 * @param c     The number of elements in the queue
 * @return      The convolution queue, or NULL if out of memory
 */
dl_convq_queue_t *dl_convq_queue_alloc(int n, int c);

/**
 * @brief Allocate a fixed-point convolution queue from PSRAM
 *
 * @param n     The length of queue
 * @param c     The number of elements in the queue
 * @return      The convolution queue, or NULL if out of memory
 */
dl_convq_queue_t *dl_convq_queue_alloc_from_psram(int n, int c);

/**
 * @brief Allocate a fixed-point multi-channel convolution queue
 *
 * @param n     The length of queue
 * @param c     The number of elements in the queue
 * @param nch   The channel of conv queue
 * @return      The convolution queue, or NULL if out of memory
 */
dl_convq_queue_t *dl_convq_queue_alloc_mc(int n, int c, int nch);

/**
 * @brief Allocate a fixed-point multi-channel convolution queue from PSRAM
 *
 * @param n     The length of queue
 * @param c     The number of elements in the queue
 * @param nch   The channel of conv queue
 * @return      The convolution queue, or NULL if out of memory
 */
dl_convq_queue_t *dl_convq_queue_alloc_mc_from_psram(int n, int c, int nch);


void dl_convq_to_matrix2dq(dl_convq_queue_t *cq, dl_matrix2dq_t* out, int row);

/**
 * @brief Free a fixed-point convolution queue
 *
 * @param cq     The fixed-point convolution queue to free
 */
void dl_convq_queue_free(dl_convq_queue_t *cq);

/**
 * @brief Set itemq of convolution queue to 0
 *
 * @param cq     The fixed-point convolution queue point
 */
void dl_convq_queue_bzero(dl_convq_queue_t *cq);

/**
 * @brief Move the front pointer of queue forward, 
          the First(oldest) element become the last(newest) element, 
 *
 * @param cq    Input fixed-point convolution queue
 * @return      Pointer of oldest element  
 */
inline qtp_t *dl_convq_queue_pop(dl_convq_queue_t *cq);
inline qtp_t *dl_convq_queue_popn(dl_convq_queue_t *cq, int n);
/**
 * @brief  Remove the oldest element, then insert the input element at the end of queue
 *
 * @param cq     Input fixed-point convolution queue
 * @param item   The new element
 */
void dl_convq_queue_push(dl_convq_queue_t *cq, dl_matrix2dq_t *a, int shift);

/**
 * @brief  Insert the float-point element at the end of queue.
 *         The precision of fixed-point numbers is described by the Qm.f notation,  
 *
 * @param cq     Input fixed-point convolution queue
 * @param item   The float-point element
 * @param m_bit  The number of integer bits including the sign bits
 * @param f_bit  The number of fractional bits
 */
void dl_convq_queue_push_by_qmf(dl_convq_queue_t *cq, fptp_t* item, int m_bit, int f_bit);

void dl_convq16_queue_push_by_qmf(dl_convq_queue_t *cq, fptp_t* item, int m_bit, int f_bit);

dl_conv_queue_t *dl_queue_from_convq(dl_convq_queue_t *cq1);

/**
 * @brief   Get the pointer of element in the queue by offset
 *
 * @param cq        Input fixed-point convolution queue
 * @param last_num  Offset from the front of the queue
 * @return          Pointer of the element
 */
inline qtp_t *dl_get_queue_itemq(dl_convq_queue_t *cq, int last_num);

/**
 * @brief   Get the pointer of element in the queue by offset
 *
 * @param cq        Input fixed-point convolution queue
 * @param offset    Offset from the front of the queue
 * @param ch        Channel index of convolution queue 
 * @return          Pointer of the element
 */
qtp_t *dl_get_queue_itemq_mc(dl_convq_queue_t *cq, int offset, int ch);

/**
 * @brief   Does a tanh operation on the one of element in the convolution queue.
 *          Gets the pointer of element in the convolution queue by offset, and does a 
 *          tanh operation by this pointer, then return the pointer 
 *
 * @param cq      Input fixed-point convolution queue
 * @param offset  Offset from the front of the queue
 * @return        Pointer of the element
 */
void dl_tanh_convq(dl_convq_queue_t *cq, int offset);

/**
 * @brief   Does a tanh operation on the one of element in multi channel convolution queue.
 *          Gets the pointer of element in the convolution queue by offset, and does a 
 *          tanh operation by this pointer, then return the pointer 
 *
 * @param cq      Input fixed-point multi channnel convolution queue
 * @param offset  Offset from the front of the queue
 * @param nch     The channel number of cqm
 * @return        Pointer of the element
 */
void dl_tanh_convq_mc(dl_convq_queue_t **cqm, int offset, int nch);

/**
 * @brief   Does a relu operation on the one of element in the convolution queue.
 *          Gets the pointer of element in the convolution queue by offset, and does a 
 *          relu operation by this pointer, then return the pointer 
 *
 * @param cq      Input fixed-point convolution queue
 * @param offset  Offset from the front of the queue
 * @return        Pointer of the element
 */
void dl_relu_convq(dl_convq_queue_t *cq, fptp_t clip, int last_num);

/**
 * @brief   Does a softmax operation on the one of element in the convolution queue.
 *          Gets the pointer of element in the convolution queue by offset, input data
            stay as it is. Results are saved into the *out* array. 
 *
 * @param cq      Input fixed-point convolution queue
 * @param offset  Offset from the front of the queue
 * @param out     Old array to re-use. Passing NULL will allocate a new matrix.
 * @return        softmax results
 */
fptp_t * dl_softmax_step_q(dl_convq_queue_t *cq, int offset, fptp_t *out);

/**
 * @brief Fast and quantised implement for 1D atrous convolution (a.k.a. convolution with holes or dilated convolution)
 *        based on convolution queue.
 *
 * @Warning All input and output convolution queue and matrix should be allocated. The return pointer
 *          is last element of output queue and should not be freed separately.
 *
 * @param in       Input fixed-point convolution queue
 * @param out      Output fixed-point convolution queue
 * @param rate     A positive int, the stride with which we sample input value 
 * @param size     A positive int, the size of 1D-filter
 * @param kernel   The kernel matrix of filter
 * @param bias     The bias matrix of filter. Can be NULL if a bias of 0 is required.
 * @param shift    Shift ratio used in dot operation between two 16-bit fixed point vector 
 * @return         The result of atrous convolution
 */
qtp_t * dl_atrous_conv1dq(dl_convq_queue_t *in, dl_convq_queue_t *out, int rate, int size,
                          dl_matrix2dq_t* kernel, dl_matrix2dq_t* bias, int shift, int prenum);

/**
 * @brief Fast implement of dilation layer as follows
 *
 *               |-> [gate(sigmoid)] -|        
 *      input -  |                    |-> (*) - output
 *               |-> [filter(tanh)]  -|   
 *
 * @Warning All input and output convolution queue and matrix should be allocated. The return pointer
 *          is last element of output queue and should not be freed separately.
 *
 * @param in              Input fixed-point convolution queue
 * @param out             Output fixed-point convolution queue
 * @param rate            A positive int, the stride with which we sample input value 
 * @param size            A positive int, the size of 1D-filter
 * @param filter_kernel   The kernel matrix of filter
 * @param filter_bias     The bias matrix of filter. Can be NULL if a bias of 0 is required.
 * @param gate_kernel     The kernel matrix of gate
 * @param gate_bias       The bias matrix of gate. Can be NULL if a bias of 0 is required.
 * @param filter_shift          Shift ratio used in filter operation between two 16-bit fixed point vector
 * @param gate_shift            Shift ratio used in gate operation between two 16-bit fixed point vector
 * @return                The result of dilation layer
 */
qtp_t *dl_dilation_layerq_steps(dl_convq_queue_t *in, dl_convq_queue_t *out, int rate, int size,
   dl_matrix2dq_t* filter_kernel, dl_matrix2dq_t* filter_bias,
   dl_matrix2dq_t* gate_kernel, dl_matrix2dq_t* gate_bias,
   int filter_shift, int gate_shift, int offset, int prenum);


qtp_t *dl_dilation_layerq(dl_convq_queue_t *in, dl_convq_queue_t *out, int rate, int size,
                          dl_matrix2dq_t* filter_kernel, dl_matrix2dq_t* filter_bias,
                          dl_matrix2dq_t* gate_kernel, dl_matrix2dq_t* gate_bias,
                          int filter_shift, int gate_shift, int prenum);

qtp_t *dl_dilation_layerq16(dl_convq_queue_t *in, dl_convq_queue_t *out, int rate, int size,
                             dl_matrix2dq_t* filter_kernel, dl_matrix2dq_t* filter_bias,
                             dl_matrix2dq_t* gate_kernel, dl_matrix2dq_t* gate_bias, int prenum);


qtp_t *dl_atrous_conv1dq_steps(dl_convq_queue_t *in, dl_convq_queue_t *out, int rate, int size,
                                 dl_matrix2dq_t* kernel, dl_matrix2dq_t* bias, int shift, int offset, int prenum);

/**
 * @brief Add a pair of fixed-point convolution queue item-by-item, and return float-point convolution queue
 *
 * @param cq1      First fixed-point convolution queue
 * @param cq2      Seconf fixed-point convolution queue
 * @return         The result of float-point convolution queue
 */
dl_conv_queue_t *dl_convq_queue_add(dl_convq_queue_t *cq1, dl_convq_queue_t *cq2);

/**
 * @brief Fast implement of LSTM layer by dl_atrous_conv1dq function
 *
 * @Warning LSTM kernel is split into two part, the first part input is the last layer output, 
 *           and kernel is parameter *in_weight*. The second part input is the last frame LSTM output,
 *           the kernel is parameters *h_weight*.
 *
 * @param in              Input fixed-point convolution queue
 * @param out             Output fixed-point convolution queue
 * @param state_c         Internal state of the LSTM network
 * @param state_h         Internal state (previous output values) of the LSTM network
 * @param in_weight       the LSTM kernel needed by first part
 * @param h_weight        the LSTM kernel needed by second part
 * @param bias            The bias matrix of LSTM. Can be NULL if a bias of 0 is required.
 * @in_shift              Shift ratio used in first part
 * @h_shift               Shift ratio used in second part
 * @return                The result of LSTM layer
 */
dl_matrix2dq_t *dl_convq_lstm_layer(const dl_convq_queue_t *in, dl_convq_queue_t *out, dl_matrix2dq_t *state_c,
                                    dl_matrix2dq_t *state_h, const dl_matrix2dq_t *in_weight, const dl_matrix2dq_t *h_weight,
                                    const dl_matrix2dq_t *bias, int in_shift, int h_shift, int prenum);
dl_matrix2dq_t *dl_basic_lstm_layer1_q(const dl_convq_queue_t *in, dl_matrix2dq_t *state_c, dl_matrix2dq_t *state_h,
                                       const dl_matrix2dq_t *weight, const dl_matrix2dq_t *bias, int step, int shift);

dl_matrix2dq_t *dl_convq16_lstm_layer(const dl_convq_queue_t *in, dl_convq_queue_t *out, dl_matrix2dq_t *state_c,
                                       dl_matrix2dq_t *state_h, const dl_matrix2dq_t *in_weight, const dl_matrix2dq_t *h_weight,
                                       const dl_matrix2dq_t *bias, int prenum);

/**
 * @brief Allocate a fixed-point multi channel convolution queue 
 *
 * @param n     The length of queue
 * @param c     The channel number of elements in the queue
 * @param nch   the channel numbet of convolution queue 
 * @return      The convolution queue, or NULL if out of memory
 */
dl_convq_queue_t **dl_convq_queue_mc_alloc(int n, int c, int nch);

/**
 * @brief Free a fixed-point multi channel convolution queue
 *
 * @param cqm     The fixed-point convolution queue to free
 * @param nch     The channel number of cqm
 */
void dl_convq_queue_mc_free(dl_convq_queue_t **cqm, int nch);

/**
 * @brief Fast and quantised implement for 1D atrous convolution (a.k.a. convolution with holes or dilated convolution)
 *        based on convolution queue.
 *
 * @Warning All input and output convolution queue and matrix should be allocated. The return pointer
 *          is last element of output queue and should not be freed separately.
 *
 * @param in       Input fixed-point convolution queue
 * @param out      Output fixed-point convolution queue
 * @param nch      The channel number of input 
 * @param rate     A positive int, the stride with which we sample input value 
 * @param size     A positive int, the size of 1D-filter
 * @param kernel   The kernel matrix of filter
 * @param bias     The bias matrix of filter. Can be NULL if a bias of 0 is required.
 * @param shift    Shift ratio used in dot operation between two 16-bit fixed point vector 
 * @param offset   the offset to calculate input convq
 * @param prenum   the preload size, 0: do not use preload function
 * @return         The result of atrous convolution
 */
qtp_t *dl_atrous_conv1dq_mc_steps(  dl_convq_queue_t **in,
                                    dl_convq_queue_t **out,
									int nch,
									int rate,
									int size,
                                    dl_matrix2dq_t* kernel,
									dl_matrix2dq_t* bias,
									int shift,
									int offset,
									int prenum);

/**
 * @brief Fast implement of dilation layer as follows for multi channel input
 *
 *               |-> [gate(sigmoid)] -|        
 *      input -  |                    |-> (*) - output
 *               |-> [filter(tanh)]  -|   
 *
 * @Warning All input and output convolution queue and matrix should be allocated. The return pointer
 *          is last element of output queue and should not be freed separately.
 *
 * @param in              Input fixed-point convolution queue
 * @param out             Output fixed-point convolution queue
 * @param nch             The channel number of input 
 * @param rate            A positive int, the stride with which we sample input value 
 * @param size            A positive int, the size of 1D-filter
 * @param filter_kernel   The kernel matrix of filter
 * @param filter_bias     The bias matrix of filter. Can be NULL if a bias of 0 is required.
 * @param gate_kernel     The kernel matrix of gate
 * @param gate_bias       The bias matrix of gate. Can be NULL if a bias of 0 is required.
 * @param filter_shift    Shift ratio used in filter operation between two 16-bit fixed point vector
 * @param gate_shift      Shift ratio used in gate operation between two 16-bit fixed point vector
 * @param offset          The offset to calculate input convq
 * @param prenum          The preload size, 0: do not use preload function
 * @return                The result of dilation layer
 */
qtp_t *dl_dilation_layerq_mc_steps( dl_convq_queue_t **in, 
									dl_convq_queue_t **out,
									int nch,
									int rate,
									int size,
                                    dl_matrix2dq_t* filter_kernel,
									dl_matrix2dq_t* filter_bias,
                                    dl_matrix2dq_t* gate_kernel,
									dl_matrix2dq_t* gate_bias,
                                    int filter_shift,
									int gate_shift,
									int offset,
									int prenum);

void test_atrous_convq(int size, int rate, int in_channel, int out_channel);
void test_lstm_convq(int size, int in_dim, int lstm_cell);
void dl_nn_tanh_i162(dl_convq_queue_t **cqm, int offset, int nch);
void dl_copy_queue_item_by_qmf(dl_convq_queue_t *cq, fptp_t* item, int m_bit, int f_bit, int offset, int ch);
void dl_convq_queue_mc_bzero(dl_convq_queue_t **cqm, int nch);
#endif