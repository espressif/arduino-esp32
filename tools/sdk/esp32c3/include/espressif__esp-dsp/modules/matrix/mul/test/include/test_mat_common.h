/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _test_mat_common_H_
#define _test_mat_common_H_

#include "dspm_mult.h"
#include "dsp_err.h"
#include "dspm_mult_platform.h"
#include "esp_dsp.h"
#include "dsp_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief data type for testing operations with sub-matrices
 * 
 * test evaluation in the test app for matrices check
 * compare 2 matrices
 */
typedef struct m_test_data_s {
    int var;
    int A_start_row;
    int A_start_col;
    int B_start_row;
    int B_start_col;
    int C_start_row;
    int C_start_col;
    int m;
    int n;
    int k;
} m_test_data_t;

/**
 * @brief check whether 2 matrices are equal
 * 
 * test evaluation in the test app for matrices check
 * compare 2 matrices
 * 
 * @param[in] m_expected: reference matrix
 * @param[in] m_actual: matrix to be evaluated
 * @param[in] message: message for test app, in case the test fails
 * 
 */
void test_assert_equal_mat_mat(dspm::Mat &m_expected, dspm::Mat &m_actual, const char *message);

/**
 * @brief check whether a matrix is set to a constant
 * 
 * test evaluation in the test app for matrices check
 * compare matrix with constant
 * 
 * @param[in] m_actual: matrix to be evaluated
 * @param[in] num: reference constant
 * @param[in] message: message for test app, if a test fails
 * 
 */
void test_assert_equal_mat_const(dspm::Mat &m_actual, float num, const char *message);

/**
 * @brief check if an area around a sub-matrix is unaffected
 * 
 * test evaluation in the test app for matrices check
 * 
 * @param[in] m_origin: original matrix
 * @param[in] m_modified: sub-matrix, which is created from m_orign
 * @param[in] start_row: sub-matrix start row
 * @param[in] start_col: sub-matrix start col
 * @param[in] message: message for test app, in case the test fails
 * 
 */
void test_assert_check_area_mat_mat(dspm::Mat &m_origin, dspm::Mat &m_modified, int start_row, int start_col, const char *message);

#ifdef __cplusplus
}
#endif

#endif // _test_mat_common_H_
