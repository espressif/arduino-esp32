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
#ifndef DL_LIB_MATRIXQ8_H
#define DL_LIB_MATRIXQ8_H

#include <stdint.h>
#include "dl_lib_matrix.h"
#include "dl_lib.h"
#include "dl_lib_matrixq.h"

typedef int8_t q8tp_t;

typedef struct {
    int w;
    int h;
    int stride; //Normally equals h, not w!
    int flags;
    int exponent; //The values in items should be multiplied by pow(2,exponent) to get the real values.
    q8tp_t *itemq;
} dl_matrix2dq8_t;

#define DL_Q8TP_SHIFT 7
#define DL_Q8TP_RANGE ((1<<DL_Q8TP_SHIFT)-1)
#define DL_ITMQ8(m, x, y) m->itemq[(y)+(x)*m->stride]

/**
 * @brief Allocate a matrix
 *
 * @param w     Width of the matrix
 * @param h     Height of the matrix
 * @return The matrix, or NULL if out of memory
 */
dl_matrix2dq8_t *dl_matrixq8_alloc(int w, int h);

/**
 * @brief Free a quantized matrix
 * Frees the matrix structure and (if it doesn't have the DL_MF_FOREIGNDATA flag set) the m->items space as well.
 *
 * @param m     Matrix to free
 */
void dl_matrixq8_free(dl_matrix2dq8_t *m);

/**
 * @brief Copy a quantized matrix
 * Copy a quantized matrix from flash or iram/psram
 *
 * @param m     Matrix to copy
 */
dl_matrix2dq8_t *dl_matrixq8_copy_to_psram(const dl_matrix2dq8_t *m);

/**
 * @brief Convert a floating-point matrix to a quantized matrix
 *
 * @param m     Floating-point matrix to convert
 * @param out   Quantized matrix to re-use. If NULL, allocate a new one.
 * @Return The quantized version of the floating-point matrix
 */
dl_matrix2dq8_t *dl_matrixq8_from_matrix2d(const dl_matrix2d_t *m, dl_matrix2dq8_t *out);

#endif