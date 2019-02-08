#ifndef DL_LIB_MATRIX_H
#define DL_LIB_MATRIX_H

typedef float fptp_t;


//Flags for matrices
#define DL_MF_FOREIGNDATA (1<<0)  /*< Matrix *item data actually points to another matrix and should not be freed */

//'Normal' float matrix
typedef struct {
    int w;          /*< Width */
    int h;          /*< Height */
    int stride;     /*< Row stride, essentially how many items to skip to get to the same position in the next row */
    int flags;      /*< Flags. OR of DL_MF_* values */
    fptp_t *item;   /*< Pointer to item array */
} dl_matrix2d_t;

//Macro to quickly access the raw items in a matrix
#define DL_ITM(m, x, y) m->item[(x)+(y)*m->stride]


//#define DL_ITM3D(m, n, x, y, z) (m)->item[(n) * (m)->stride * (m)->c + (z) * (m)->stride + (y) * (m)->w + (x)]

/**
 * @brief Allocate a matrix
 *
 * @param w     Width of the matrix
 * @param h     Height of the matrix
 * @return The matrix, or NULL if out of memory
 */
dl_matrix2d_t *dl_matrix_alloc(int w, int h);


/**
 * @brief Free a matrix
 * Frees the matrix structure and (if it doesn't have the DL_MF_FOREIGNDATA flag set) the m->items space as well.
 *
 * @param m     Matrix to free
 */
void dl_matrix_free(dl_matrix2d_t *m);

/**
 * @brief Zero out the matrix
 * Sets all entries in the matrix to 0.
 *
 * @param m     Matrix to zero
 */
void dl_matrix_zero(dl_matrix2d_t *m);

/**
 * @brief Generate a new matrix using a range of items from an existing matrix.
 * When using this, the data of the new matrix is not allocated/copied but it re-uses a pointer
 * to the existing data. Changing the data in the resulting matrix, as a result, will also change
 * the data in the existing matrix that has been sliced.
 *
 * @param x     X-offset of the origin of the returned matrix within the sliced matrix
 * @param y     Y-offset of the origin of the returned matrix within the sliced matrix
 * @param w     Width of the resulting matrix
 * @param h     Height of the resulting matrix
 * @param in    Old matrix (with foreign data) to re-use. Passing NULL will allocate a new matrix.
 * @return The resulting slice matrix, or NULL if out of memory
 */
dl_matrix2d_t *dl_matrix_slice(const dl_matrix2d_t *src, int x, int y, int w, int h, dl_matrix2d_t *in);

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
 * @return  The resulting flatten matrix, or NULL if out of memory
 */
dl_matrix2d_t *dl_matrix_flatten(const dl_matrix2d_t *src, int x, int y, int w, int h, dl_matrix2d_t *in);

/**
 * @brief Generate a matrix from existing floating-point data
 *
 * @param w     Width of resulting matrix
 * @param h     Height of resulting matrix
 * @param data  Data to populate matrix with
 * @return A newaly allocated matrix populated with the given input data, or NULL if out of memory.
 */
dl_matrix2d_t *dl_matrix_from_data(int w, int h, int stride, const void *data);


/**
 * @brief Multiply a pair of matrices item-by-item: res=a*b
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Multiplicated data. Can be equal to a or b to overwrite that.
 */
void dl_matrix_mul(const dl_matrix2d_t *a, const dl_matrix2d_t *b, dl_matrix2d_t *res);

/**
 * @brief Do a dotproduct of two matrices : res=a.b
 *
 * @param a     First multiplicand
 * @param b     Second multiplicand
 * @param res   Dotproduct data. *Must* be a *different* matrix from a or b!
 */
void dl_matrix_dot(const dl_matrix2d_t *a, const dl_matrix2d_t *b, dl_matrix2d_t *res);

/**
 * @brief Add a pair of matrices item-by-item: res=a-b
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @param res   Added data. Can be equal to a or b to overwrite that.
 */
void dl_matrix_add(const dl_matrix2d_t *a, const dl_matrix2d_t *b, dl_matrix2d_t *out);


/**
 * @brief Divide a pair of matrices item-by-item: res=a/b
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @param res   Divided data. Can be equal to a or b to overwrite that.
 */
void dl_matrix_div(const dl_matrix2d_t *a, const dl_matrix2d_t *b, dl_matrix2d_t *out);

/**
 * @brief Subtract a matrix from another, item-by-item: res=a-b
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @param res   Subtracted data. Can be equal to a or b to overwrite that.
 */
void dl_matrix_sub(const dl_matrix2d_t *a, const dl_matrix2d_t *b, dl_matrix2d_t *out);

/**
 * @brief Add a constant to every item of the matrix
 *
 * @param subj  Matrix to add the constant to
 * @param add   The constant
 */
void dl_matrix_add_const(dl_matrix2d_t *subj, const fptp_t add);


/**
 * @brief Concatenate the rows of two matrices into a new matrix
 *
 * @param a     First matrix
 * @param b     Second matrix
 * @return A newly allocated array with as avlues a|b
 */
dl_matrix2d_t *dl_matrix_concat(const dl_matrix2d_t *a, const dl_matrix2d_t *b);


/**
 * @brief Print the contents of a matrix to stdout. Used for debugging.
 *
 * @param a     The matrix to print.
 */
void dl_printmatrix(const dl_matrix2d_t *a);

/**
 * @brief Return the average square error given a correct and a test matrix.
 *
 * ...Well, more or less. If anything, it gives an indication of the error between
 * the two. Check the code for the exact implementation.
 *
 * @param a     First of the two matrices to compare
 * @param b     Second of the two matrices to compare
 * @return value indicating the relative difference between matrices
 */
float dl_matrix_get_avg_sq_err(const dl_matrix2d_t *a, const dl_matrix2d_t *b);



/**
 * @brief Check if two matrices have the same shape, that is, the same amount of rows and columns
 *
 * @param a     First of the two matrices to compare
 * @param b     Second of the two matrices to compare
 * @return true if the two matrices are shaped the same, false otherwise.
 */
int dl_matrix_same_shape(const dl_matrix2d_t *a, const dl_matrix2d_t *b);


/**
 * @brief Get a specific item from the matrix
 *
 * Please use these for external matrix access instead of DL_ITM
 *
 * @param m     Matrix to access
 * @param x     Column address
 * @param y     Row address
 * @return Value in that position
 */
inline static fptp_t dl_matrix_get(const dl_matrix2d_t *m, const int x, const int y) { 
    return DL_ITM(m, x, y);
}

/**
 * @brief Set a specific item in the matrix to the given value
 *
 * Please use these for external matrix access instead of DL_ITM
 *
 * @param m     Matrix to access
 * @param x     Column address
 * @param y     Row address
 * @param val   Value to write to that position
 */
inline static void dl_matrix_set(dl_matrix2d_t *m, const int x, const int y, fptp_t val) { 
    DL_ITM(m, x, y)=val;
}

#endif

