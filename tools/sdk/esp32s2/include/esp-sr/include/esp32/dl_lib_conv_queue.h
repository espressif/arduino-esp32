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
#ifndef DL_LIB_CONV_QUEUE_H
#define DL_LIB_CONV_QUEUE_H


#include "dl_lib_matrix.h"
typedef float fptp_t;


//Flags for matrices
#define DL_MF_FOREIGNDATA (1<<0)  /*< Matrix *item data actually points to another matrix and should not be freed */

//Float convolution FIFO queue. 
typedef struct {
    int n;          /*< the length of queue */
    int c;          /*< the channel number of queue element*/
    int front;      /*< the front(top) position of queue */
    int flag;       /*< not used*/
    fptp_t *item;   /*< Pointer to item array */
} dl_conv_queue_t;

/**
 * @brief Allocate a convolution queue
 *
 * @param n     The length of queue
 * @param c     The channel number of elements in the queue
 * @return      The convolution queue, or NULL if out of memory
 */
dl_conv_queue_t *dl_conv_queue_alloc(int n, int c);

/**
 * @brief Free a convolution queue
 *
 * @param cq     The convolution queue to free
 */
void dl_conv_queue_free(dl_conv_queue_t *cq);

void dl_conv_to_matrix2d(dl_conv_queue_t *cq, dl_matrix2d_t* out);

/**
 * @brief Move the front pointer of queue forward, 
          the First(oldest) element become the last(newest) element, 
 *
 * @param cq    Input convolution queue
 * @return      Pointer of oldest element  
 */
fptp_t *dl_conv_queue_pop(dl_conv_queue_t *cq);

/**
 * @brief  Remove the oldest element, then insert the input element at the end of queue
 *
 * @param cq     Input convolution queue
 * @param item   The new element
 */
void dl_conv_queue_push(dl_conv_queue_t *cq, fptp_t* item);


/**
 * @brief   Get the pointer of element in the queue by offset
 *
 * @param cq      Input convolution queue
 * @param offset  Offset from the front of the queue
 * @return        Pointer of the element
 */
fptp_t *dl_get_queue_item(dl_conv_queue_t *cq, int offset);

/**
 * @brief   Does a sigmoid operation on the one of element in the convolution queue.
 * Gets the pointer of element in the convolution queue by offset, and does a sigmoid operation
 * by this pointer, then return the pointer      
 *
 * @param cq      Input convolution queue
 * @param offset  Offset from the front of the queue
 * @return        Pointer of the element
 */
fptp_t *dl_sigmoid_step(dl_conv_queue_t *cq, int offset);

/**
 * @brief   Does a tanh operation on the one of element in the convolution queue.
 * Gets the pointer of element in the convolution queue by offset, and does a tanh operation
 * by this pointer, then return the pointer  
 *
 * @param cq      Input convolution queue
 * @param offset  Offset from the front of the queue
 * @return        Pointer of the element
 */
fptp_t *dl_tanh_step(dl_conv_queue_t *cq, int offset);

/**
 * @brief   Does a softmax operation on the one of element in the convolution queue.
 * Gets the pointer of element in the convolution queue by offset, and does a softmax operation
 * by this pointer, then return the pointer 
 *
 * @param cq      Input convolution queue
 * @param offset  Offset from the front of the queue
 * @return        Pointer of the element
 */
fptp_t *dl_softmax_step(dl_conv_queue_t *cq, int offset);

fptp_t *dl_relu_step(dl_conv_queue_t *cq, int offset);
fptp_t *dl_relu_look(dl_matrix2d_t *cq, int offset);
dl_matrix2d_t *dl_matrix_concat1(const dl_conv_queue_t *a, const dl_matrix2d_t *b);
dl_matrix2d_t *dl_basic_lstm_layer1(const dl_conv_queue_t *in, dl_matrix2d_t *state_c, dl_matrix2d_t *state_h,
                                   const dl_matrix2d_t *weight, const dl_matrix2d_t *bias);
/**
 * @brief Fast implement for 1D atrous convolution (a.k.a. convolution with holes or dilated convolution)
 *        based on convolution queue.
 *
 * @Warning All input and output convolution queue and matrix should be allocated. The return pointer
 *          is first element of output queue and should not be freed separately.
 *
 * @param in       Input convolution queue
 * @param out      Output convolution queue
 * @param rate     A positive int, the stride with which we sample input value 
 * @param size     A positive int, the size of 1D-filter
 * @param kernel   The kernel matrix of filter
 * @param bias     The bias matrix of filter. Can be NULL if a bias of 0 is required.
 * @return         The result of atrous convolution
 */
fptp_t *dl_atrous_conv1d_step(dl_conv_queue_t *in, dl_conv_queue_t *out, int rate, int size,
                              dl_matrix2d_t* kernel, dl_matrix2d_t* bias);
fptp_t *dl_look_conv_step(dl_conv_queue_t *in, dl_matrix2d_t *out, int rate, int size,
                         dl_matrix2d_t* kernel, dl_matrix2d_t* bias);

/**
 * @brief Fast implement of dilation layer as follows
 *
 *               |-> [gate(sigmoid)] -|        
 *      input -  |                    |-> (*) - output
 *               |-> [filter(tanh)]  -|   
 *
 * @Warning All input and output convolution queue and matrix should be allocated. The return pointer
 *          is first element of output queue and should not be freed separately.
 *
 * @param in              Input convolution queue
 * @param out             Output convolution queue
 * @param rate            A positive int, the stride with which we sample input value 
 * @param size            A positive int, the size of 1D-filter
 * @param filter_kernel   The kernel matrix of filter
 * @param filter_bias     The bias matrix of filter. Can be NULL if a bias of 0 is required.
 * @param gate_kernel     The kernel matrix of gate
 * @param gate_bias       The bias matrix of gate. Can be NULL if a bias of 0 is required.
 * @return                The result of dilation layer
 */
fptp_t *dl_dilation_layer(dl_conv_queue_t *in, dl_conv_queue_t *out, int rate, int size,
                          dl_matrix2d_t* filter_kernel, dl_matrix2d_t* filter_bias,
                          dl_matrix2d_t* gate_kernel, dl_matrix2d_t* gate_bias);


void test_atrous_conv(int size, int rate, int in_channel, int out_channel);

#endif