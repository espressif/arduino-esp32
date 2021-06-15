// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _dspm_mat_h_
#define _dspm_mat_h_
#include <iostream>

/**
 * @brief   DSP matrix namespace
 *
 * DSP library matrix namespace.
 */
namespace dspm {
/**
 * @brief   Matrix
 *
 * The Mat class provides basic matrix operations on single-precision floating point values.
 */
class Mat {
public:
    /**
     * Constructor allocate internal buffer.
     * @param[in] rows: amount of matrix rows
     * @param[in] cols: amount of matrix columns
     */
    Mat(int rows, int cols);
    /**
    * Constructor use external buffer.
     * @param[in] data: external buffer with row-major matrix data
     * @param[in] rows: amount of matrix rows
     * @param[in] cols: amount of matrix columns
    */
    Mat(float *data, int rows, int cols);
    /**
     * Allocate matrix with undefined size.
     */
    Mat();
    virtual ~Mat();
    /**
     * Make copy of matrix.
     * @param[in] src: source matrix
     */
    Mat(const Mat &src);
    /**
     * Copy operator
     *
     * @param[in] src: source matrix
     *
     * @return
     *      - matrix copy
     */
    Mat &operator=(const Mat &src);

    bool ext_buff; /*!< Flag indicates that matrix use external buffer*/

    /**
     * Access to the matrix elements.
     * @param[in] row: row position
     * @param[in] col: column position
     *
     * @return
     *      - element of matrix M[row][col]
     */
    inline float &operator()(int row, int col)
    {
        return data[row * this->cols + col];
    }
    /**
     * Access to the matrix elements.
     * @param[in] row: row position
     * @param[in] col: column position
     *
     * @return
     *      - element of matrix M[row][col]
     */
    inline const float &operator()(int row, int col) const
    {
        return data[row * this->cols + col];
    }

    /**
     * += operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] A: source matrix
     *
     * @return
     *      - result matrix: result += A
     */
    Mat &operator+=(const Mat &A);

    /**
     * += operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] C: constant
     *
     * @return
     *      - result matrix: result += C
     */
    Mat &operator+=(float C);
    /**
     * -= operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] A: source matrix
     *
     * @return
     *      - result matrix: result -= A
     */
    Mat &operator-=(const Mat &A);

    /**
     * -= operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] C: constant
     *
     * @return
     *      - result matrix: result -= C
     */
    Mat &operator-=(float C);

    /**
     * *= operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] A: source matrix
     *
     * @return
     *      - result matrix: result -= A
     */
    Mat &operator*=(const Mat &A);
    /**
     * += with constant operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] C: constant value
     *
     * @return
     *      - result matrix: result *= C
     */
    Mat &operator*=(float C);
    /**
     * /= with constant operator
     * The operator use DSP optimized implementation of multiplication.
     *
     * @param[in] C: constant value
     *
     * @return
     *      - result matrix: result /= C
     */
    Mat &operator/=(float C);
    /**
     * /= operator
     *
     * @param[in] B: source matrix
     *
     * @return
     *      - result matrix: result[i,j] = result[i,j]/B[i,j]
     */
    Mat &operator/=(const Mat &B);
    /**
     * ^= xor with constant operator
     * The operator use DSP optimized implementation of multiplication.
     * @param[in] C: constant value
     *
     * @return
     *      - result matrix: result ^= C
     */
    Mat  operator^(int C);

    /**
     * Swap two rows between each other.
     * @param[in] row1: position of first row
     * @param[in] row2: position of second row
     */
    void swapRows(int row1, int row2);
    /**
     * Matrix transpose.
     * Change rows and columns between each other.
     *
     * @return
     *      - transposed matrix
     */
    Mat t();

    /**
     * Create identity matrix.
     * Create a square matrix and fill diagonal with 1.
     *
     * @param[in] size: matrix size
     *
     * @return
     *      - matrix [N]x[N] with 1 in diagonal
     */
    static Mat eye(int size);

    /**
     * Create matrix with all elements 1.
     * Create a square matrix and fill all elements with 1.
     *
     * @param[in] size: matrix size
     *
     * @return
     *      - matrix [N]x[N] with 1 in all elements
     */
    static Mat ones(int size);

    /**
     * Return part of matrix from defined position (startRow, startCol) as a matrix[blockRows x blockCols].
     *
     * @param[in] startRow: start row position
     * @param[in] startCol: start column position
     * @param[in] blockRows: amount of rows in result matrix
     * @param[in] blockCols: amount of columns in the result matrix
     *
     * @return
     *      - matrix [blockRows]x[blockCols]
     */
    Mat block(int startRow, int startCol, int blockRows, int blockCols);

    /**
     * Normalizes the vector, i.e. divides it by its own norm.
     * If it's matrix, calculate matrix norm
     *
     */
    void normalize(void);

    /**
     * Return  norm of the vector.
     * If it's matrix, calculate matrix norm
     *
     * @return
     *      - matrix norm
     */
    float norm(void);

    /**
     * The method fill 0 to the matrix structure.
     *
     */
    void clear(void);

    /**
     * @brief   Solve the matrix
     *
     * Solve matrix. Find roots for the matrix A*x = b
     *
     * @param[in] A: matrix [N]x[N] with input coefficients
     * @param[in] b: vector [N]x[1] with result values
     *
     * @return
     *      - matrix [N]x[1] with roots
     */
    static Mat solve(Mat A, Mat b);
    /**
     * @brief   Band solve the matrix
     *
     * Solve band matrix. Find roots for the matrix A*x = b with bandwidth k.
     *
     * @param[in] A: matrix [N]x[N] with input coefficients
     * @param[in] b: vector [N]x[1] with result values
     * @param[in] k: upper bandwidth value
     *
     * @return
     *      - matrix [N]x[1] with roots
     */
    static Mat bandSolve(Mat A, Mat b, int k);
    /**
     * @brief   Solve the matrix
     *
     * Different way to solve the matrix. Find roots for the matrix A*x = y
     *
     * @param[in] A: matrix [N]x[N] with input coefficients
     * @param[in] y: vector [N]x[1] with result values
     *
     * @return
     *      - matrix [N]x[1] with roots
     */
    static Mat roots(Mat A, Mat y);

    /**
     * @brief   Dotproduct of two vectors
     *
     * The method returns dotproduct of two vectors
     *
     * @param[in] A: Input vector A Nx1
     * @param[in] B: Input vector B Nx1
     *
     * @return
     *      - dotproduct value
     */
    static float dotProduct(Mat A, Mat B);

    /**
     * @brief   Augmented matrices
     *
     * Augmented matrices
     *
     * @param[in] A: Input vector A MxN
     * @param[in] B: Input vector B MxK
     *
     * @return
     *      - Augmented matrix Mx(N+K)
     */
    static Mat augment(Mat A, Mat B);
    /**
     * @brief   Gaussian Elimination
     *
     * Gaussian Elimination of matrix
     *
     * @return
     *      - result matrix
     */
    Mat gaussianEliminate();

    /**
     * Row reduction for Gaussian elimination
     *
     * @return
     *      - result matrix
     */
    Mat rowReduceFromGaussian();

    /**
     * Find the inverse matrix
     *
     * @return
     *      - inverse matrix
     */
    Mat inverse();

    /**
     * Find pseudo inverse matrix
     *
     * @return
     *      - inverse matrix
     */
    Mat pinv();

    int rows; /*!< Amount of rows*/
    int cols; /*!< Amount of columns*/
    float *data; /*!< Buffer with matrix data*/
    int length; /*!< Total amount of data in data array*/

    static float abs_tol; /*!< Max acceptable absolute tolerance*/
private:
    Mat cofactor(int row, int col, int n);
    float det(int n);
    Mat adjoint();

    void allocate(); // Allocate buffer
    Mat expHelper(const Mat &m, int num);
};
/**
 * Print matrix to the standard iostream.
 * @param[in] os: output stream
 * @param[in] m: matrix to print
 *
 * @return
 *      - output stream
 */
std::ostream &operator<<(std::ostream &os, const Mat &m);
/**
 * Fill the matrix from iostream.
 * @param[in] is: input stream
 * @param[in] m: matrix to fill
 *
 * @return
 *      - input stream
 */
std::istream &operator>>(std::istream &is, Mat &m);

/**
 * + operator, sum of two matrices
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] B: Input matrix B
 *
 * @return
 *     - result matrix A+B
*/
Mat operator+(const Mat &A, const Mat &B);
/**
 * + operator, sum of matrix with constant
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] C: Input constant
 *
 * @return
 *     - result matrix A+C
*/
Mat operator+(const Mat &A, float C);

/**
 * - operator, subtraction of two matrices
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] B: Input matrix B
 *
 * @return
 *     - result matrix A-B
*/
Mat operator-(const Mat &A, const Mat &B);
/**
 * - operator, sum of matrix with constant
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] C: Input constant
 *
 * @return
 *     - result matrix A+C
*/
Mat operator-(const Mat &A, float C);

/**
 * * operator, multiplication of two matrices.
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] B: Input matrix B
 *
 * @return
 *     - result matrix A*B
*/
Mat operator*(const Mat &A, const Mat &B);

/**
 * * operator, multiplication of matrix with constant
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] C: floating point value
 *
 * @return
 *     - result matrix A*B
*/
Mat operator*(const Mat &A, float C);

/**
 * * operator, multiplication of matrix with constant
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] C: floating point value
 * @param[in] A: Input matrix A
 *
 * @return
 *     - result matrix A*B
*/
Mat operator*(float C, const Mat &A);

/**
 * / operator, divide of matrix by constant
 * The operator use DSP optimized implementation of multiplication.
 *
 * @param[in] A: Input matrix A
 * @param[in] C: floating point value
 *
 * @return
 *     - result matrix A*B
*/
Mat operator/(const Mat &A, float C);

/**
 * / operator, divide matrix A by matrix B
 *
 * @param[in] A: Input matrix A
 * @param[in] B: Input matrix B
 *
 * @return
 *     - result matrix C, where C[i,j] = A[i,j]/B[i,j]
*/
Mat operator/(const Mat &A, const Mat &B);

/**
 * == operator, compare two matrices
 *
 * @param[in] A: Input matrix A
 * @param[in] B: Input matrix B
 *
 * @return
 *      - true if matrices are the same
 *      - false if matrices are different
*/
bool operator==(const Mat &A, const Mat &B);

}
#endif //_dspm_mat_h_
