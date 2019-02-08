#ifndef DL_LIB_MATRIXQ_H
#define DL_LIB_MATRIXQ_H

#include <stdint.h>
#include "dl_lib_matrix.h"

typedef int16_t qtp_t;

//Quantized matrix. Uses fixed numbers and has the storage for the rows/columns inverted 
//for easy use as a multiplicand without stressing out the flash cache too much.
typedef struct {
    int w;
    int h;
    int stride; //Normally equals h, not w!
    int flags;
    int exponent; //The values in items should be multiplied by pow(2,exponent) to get the real values.
    qtp_t *itemq;
} dl_matrix2dq_t;

#define DL_QTP_SHIFT 15
#define DL_QTP_RANGE ((1<<DL_QTP_SHIFT)-1)
#define DL_ITMQ(m, x, y) m->itemq[(y)+(x)*m->stride]
#define DL_QTP_EXP_NA 255 //non-applicable exponent because matrix is null

#define DL_SHIFT_AUTO 32

/**
 * @info About quantized matrices and shift values
 *
 * Grab a coffee (or tea, or hot water)  and sit down when you read this for the first 
 * time. Quantized matrices can speed up your operations, but come with some quirks, and
 * it's good to understand how they work before using them.
 *
 * The data in the quantized matrix type is stored similarily to floating-point types:
 * when storing a real value, the value is stored as a mantissa (base number) and an
 * exponent. The 'real' value that can be re-derived from those two numbers is something
 * similar to mantissa*2^exponent. Up to this point, there's not that much difference from 
 * the standard floating point implementations like e.g. IEEE-754.
 *
 * The difference with respect to quantized matrices is that for a quantized matrix, it is 
 * assumed all values stored have more-or-less the same order of magnitude. This allows the
 * matrix to only store all the mantissas, while the exponents are shared; there is only one 
 * exponent for the entire matrix. This makes it quicker to handle matrix operations - the
 * logic to fix the exponents only needs to happen once, while the rest can be done in simple
 * integer arithmetic. It also nets us some memory savings - while normally a floating point
 * number is 32-bit, storing only 16-bit mantissas as the matrix items almost halves the 
 * memory requirements.
 *
 * While most of the details of handling the intricacies of the quantized matrixes are done
 * transparently by the code in dl_lib_matrixq.c, some implementation details leak out, 
 * specifically in places where addition/subtraction/division happens.
 *
 * The problem is that the routines do not know what the size of the resulting operation is. For
 * instance, when adding two matrices of numbers, the resulting numbers *could* be large enough
 * to overflow the mantissa of the result if the exponent is the same. However, if by default we
 * assume the mantissas needs to be scaled back, we may lose precision.
 *
 * In order to counter this, all operations that have this issue have a ``shift`` argument. If 
 * the argument is zero, the routine will be conservative, that is, increase the exponent of 
 * the result to such an extent it's mathematically impossible a value in the result will exceed
 * the maximum value that can be stored. However, when this argument is larger than zero, the
 * algorithm will hold back on this scaling by the indicated amount of bits, preserving precision
 * but increasing the chance of some of the calculated values not fitting in the mantissa anymore.
 * If this happens, the value will be clipped to the largest (or, for negative values, smallest)
 * value possible. (Neural networks usually are okay with this happening for a limited amount
 * of matrix indices).
 *
 * For deciding on these shift values, it is recommended to start with a shift value of one, then
 * use dl_matrixq_check_sanity on the result. If this indicates clipping, lower the shift value. 
 * If it indicates bits are under-used, increase it. Note that for adding and subtraction, only
 * shift values of 0 or 1 make sense; these routines will error out if you try to do something
 * else.
 *
 * For neural networks and other noise-tolerant applications, note that even when 
 * dl_matrixq_check_sanity does not indicate any problems, twiddling with the shift value may lead
 * to slightly improved precision. Feel free to experiment.
 **/


/**
 * @brief Allocate a matrix
 *
 * @param w     Width of the matrix
 * @param h     Height of the matrix
 * @return The matrix, or NULL if out of memory
 */
dl_matrix2dq_t *dl_matrixq_alloc(int w, int h);

/**
 * @brief Convert a floating-point matrix to a quantized matrix
 *
 * @param m     Floating-point matrix to convert
 * @param out   Quantized matrix to re-use. If NULL, allocate a new one.
 * @Return The quantized version of the floating-point matrix
 */
dl_matrix2dq_t *dl_matrixq_from_matrix2d(const dl_matrix2d_t *m, dl_matrix2dq_t *out);


/**
 * TODO: DESCRIBE THIS FUNCTION
 */
dl_matrix2dq_t *dl_matrixq_from_matrix2d_by_qmf(const dl_matrix2d_t *m, dl_matrix2dq_t *out, int m_bit, int f_bit);


/**
 * @brief Convert a quantized matrix to a floating-point one.
 *
 * @param m     Floating-point matrix to convert
 * @param out   Quantized matrix to re-use. If NULL, allocate a new one.
 * @Return The quantized version of the floating-point matrix
 **/
dl_matrix2d_t *dl_matrix2d_from_matrixq(const dl_matrix2dq_t *m, dl_matrix2d_t *out);


/**
 * @brief Free a quantized matrix
 * Frees the matrix structure and (if it doesn't have the DL_MF_FOREIGNDATA flag set) the m->items space as well.
 *
 * @param m     Matrix to free
 */
void dl_matrixq_free(dl_matrix2dq_t *m);

/**
 * @brief Zero out the matrix
 * Sets all entries in the matrix to 0.
 *
 * @param m     Matrix to zero
 */
void dl_matrixq_zero(dl_matrix2dq_t *m);


/**
 * @brief Do a dotproduct of two quantized matrices : res=a.b, Result is a fixed-point matrix.
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Dotproduct data. *Must* be a *different* matrix from a or b!
 * @param shift Shift ratio
 */
void dl_matrixq_dot(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2dq_t *res, int shift);

/**
 * @brief Do a dotproduct of two quantized matrices: res=a.b, Result is a floating-point matrix.
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Dotproduct data. *Must* be a *different* matrix from a or b!
 */
void dl_matrixq_dot_matrix_out(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2d_t *res);

/**
 * @brief Do a dotproduct of two quantized matrices : res=a.b. This always uses the simple & stupid C algo for the dot product.
 *
 * Result is a fixed-point matrix. 
 *
 * Use this only if you expect something is wrong with the accelerated routines that dl_matrixq_dot calls; this function can be
 * much slower than dl_matrixq_dot .
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Dotproduct data. *Must* be a *different* matrix from a or b!
 * @param shift Shift ratio
 */
void dl_matrixq_dot_c_impl(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2dq_t *res, int shift);

/**
 * @brief Do a dotproduct of two quantized matrices : res=a.b. This always uses the simple & stupid C algo for the dot product. 
 *
 * Result is a floating-point matrix. 
 *
 * Use this only if you expect something is wrong with the accelerated routines that dl_matrixq_dot_matrix_out calls; this function can be
 * much slower than dl_matrixq_dot_matrix_out.
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Dotproduct data. *Must* be a *different* matrix from a or b!
 */
void dl_matrixq_dot_matrix_out_c_impl(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2d_t *res);

/**
 * @brief Do a dotproduct of a floating point and a quantized matrix. Result is a floating-point matrix.
 *
 * @param a     First multiplicand; float matrix
 * @param b     Second multiplicand; quantized matrix
 * @param res   Dotproduct data; float matrix. *Must* be a *different* matrix from a or b!
 */
void dl_matrix_matrixq_dot(const dl_matrix2d_t *a, const dl_matrix2dq_t *b, dl_matrix2d_t *res);


/**
 * @brief Print the contents of a quantized matrix to stdout. Used for debugging.
 *
 * @param a     The matrix to print.
 */
void dl_printmatrixq(const dl_matrix2dq_t *a);


/**
 * @brief Add a pair of quantizedmatrices item-by-item: res=a-b
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @param res   Added data. Can be equal to a or b to overwrite that.
 * @param shift Shift value. Only 0 or 1 makes sense here. <ToDo: check>
 */
void dl_matrixq_add(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2dq_t *res, int shift);

/**
 * @brief Generate a new matrix using a range of items from an existing matrix.
 * When using this, the data of the new matrix is not allocated/copied but it re-uses a pointer
 * to the existing data. Changing the data in the resulting matrix, as a result, will also change
 * the data in the existing matrix that has been sliced.
 *
 * @Warning In contrast to the floating point equivalent of this function, the fixed-point version
 * of this has the issue that as soon as the output exponent of one of the slices changes, the data
 * in the sliced matrix gets corrupted (because the exponent of that matrix is still the same.) If you
 * use this function, either treat the slices as read-only, or assume the sliced matrix contains
 * garbage after modifying the data in one of the slices.
 *
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 * @param in    Old matrix (with foreign data) to re-use. Passing NULL will allocate a new matrix.
 * @return The resulting slice matrix, or NULL if out of memory
 */
dl_matrix2dq_t *dl_matrixq_slice(const dl_matrix2dq_t *src, int x, int y, int w, int h, dl_matrix2dq_t *in);

/**
 * @brief select a range of items from an existing matrix and flatten them into one dimension.
 *
 * @Warning The results are flattened in row-major order.
 *   
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 * @param in    Old matrix to re-use. Passing NULL will allocate a new matrix.
 * @return The resulting flatten matrix, or NULL if out of memory
 */
dl_matrix2dq_t *dl_matrixq_flatten(const dl_matrix2dq_t *src, int x, int y, int w, int h, dl_matrix2dq_t *in);

/**
 * @brief Subtract a quantized matrix from another, item-by-item: res=a-b
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @param res   Subtracted data. Can be equal to a or b to overwrite that.
 * @param shift Shift value. Only 0 or 1 makes sense here. <ToDo: check>
 */
void dl_matrixq_sub(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2dq_t *res, int shift);

/**
 * @brief Multiply a pair of quantized matrices item-by-item: res=a*b
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Multiplicated data. Can be equal to a or b to overwrite that matrix.
 */
void dl_matrixq_mul(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2dq_t *res);

/**
 * @brief Divide a pair of quantized matrices item-by-item: res=a/b
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @param res   Divided data. Can be equal to a or b to overwrite that.
 */
void dl_matrixq_div(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b, dl_matrix2dq_t *out, int shift);

/**
 * @brief Check if two quantized matrices have the same shape, that is, the same amount of 
 * rows and columns
 *
 * @param a     First of the two matrices to compare
 * @param b     Second of the two matrices to compare
 * @return true if the two matrices are shaped the same, false otherwise.
 */
int dl_matrixq_same_shape(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b);

/**
 * @brief Concatenate the rows of two quantized matrices into a new matrix
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @return A newly allocated quantized matrix with as values a|b
 */
dl_matrix2dq_t *dl_matrixq_concat(const dl_matrix2dq_t *a, const dl_matrix2dq_t *b);

/**
 * @brief Add a constant to every item of the quantized matrix
 *
 * @param subj  Matrix to add the constant to
 * @param add   The constant
 */
void dl_matrixq_add_const(dl_matrix2dq_t *subj, const fptp_t add, int shift);

/**
 * @brief Check the sanity of a quantized matrix
 *
 * Due to the nature of quantized matrices, depending on the calculations a quantized
 * matrix is the result of and the shift values chosen in those calculations, a quantized
 * matrix may have an exponent and mantissas that lead to a loss of precision, either because
 * most significant mantissa bits are unused, or because a fair amount of mantissas are 
 * clipped. This function checks if this is the case and will report a message to stdout
 * if significant loss of precision is detected.
 *
 * @param m     The quantized matrix to check
 * @param name  A string to be displayed in the message if the sanity check fails
 * @return True if matrix is sane, false otherwise
 **/

int dl_matrixq_check_sanity(dl_matrix2dq_t *m, const char *name);

/**
 * @brief re-adjust the exponent of the matrix to fit the mantissa better
 *
 * This function will shift up all the data in the mantissas so there are no
 * most-significant bits that are unused in all mantissas. It will also adjust
 * the exponent to keep the actua values in the matrix the same.
 *
 * Some operations done on a matrix, especially operations that re-use the
 * result of earlier operations done in the same way, can lead to the loss of
 * data because the exponent of the quantized matrix is never re-adjusted. You
 * can do that implicitely by calling this function.
 *
 * @param m     The matrix to re-adjust
**/
void dl_matrixq_readjust_exp(dl_matrix2dq_t *m);



/**
 * @brief Get the floating-point value of a specific item from the quantized matrix
 *
 * @param m     Matrix to access
 * @param x     Column address
 * @param y     Row address
 * @return Value in that position
 */
fptp_t dl_matrixq_get(const dl_matrix2dq_t *m, const int x, const int y);

/**
 * @brief Set a specific item in the quantized matrix to the given 
 * floating-point value
 *
 * @warning If the given value is more than the exponent in the quantized matrix
 * allows for, all mantissas in the matrix will be shifted down to make the value
 * 'fit'. If, however, the exponent is such that the value would result in a
 * quantized mantissa of 0, nothing is done.
 *
 * @param m     Matrix to access
 * @param x     Column address
 * @param y     Row address
 * @param val   Value to write to that position
 */
void dl_matrixq_set(dl_matrix2dq_t *m, const int x, const int y, fptp_t val);

#endif
