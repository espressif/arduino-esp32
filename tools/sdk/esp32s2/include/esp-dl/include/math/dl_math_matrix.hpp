#pragma once

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include "dl_define.hpp"
#include "dl_tool.hpp"
#include "dl_variable.hpp"
#include "esp_timer.h"

namespace dl
{
    namespace math
    {
        /**
         * @brief the Matrix class
         * 
         * @tparam T 
         */
        template <typename T>
        class Matrix
        {
        public:
            T **array;
            int h;
            int w;
            Matrix() : h(0), w(0)
            {
                this->array = NULL;
            }

            Matrix(int h, int w) : h(h), w(w)
            {
                this->calloc_element();
            }

            Matrix(int h, int w, T s) : h(h), w(w)
            {
                this->calloc_element();
                this->set_value(s);
            }

            Matrix(const Matrix<T> &mat) : h(mat.h), w(mat.w)
            {
                this->calloc_element();
                this->set_value(mat);
            }
            virtual ~Matrix()
            {
                if (this->array != NULL)
                {
                    for (int i = 0; i < this->h; i++)
                    {
                        free(this->array[i]);
                    }
                    free(this->array);
                    this->array = NULL;
                }
            }

            /**
             * @brief calloc the matrix element 
             * 
             */
            void calloc_element()
            {
                if ((this->h > 0) && (this->w > 0))
                {
                    this->array = (T **)calloc(this->h, sizeof(T *));
                    for (int i = 0; i < this->h; i++)
                    {
                        this->array[i] = (T *)calloc(this->w, sizeof(T));
                    }
                }
                else
                {
                    this->array = NULL;
                }
            }

            /**
             * @brief Set the matrix element to random number.
             * 
             * @param thresh the max abs value of the element. 
             */
            void set_random(T thresh = 1)
            {
                unsigned int seed = esp_timer_get_time();
                srand(seed);
                for (int i = 0; i < this->h; i++)
                {
                    for (int j = 0; j < this->w; j++)
                    {
                        this->array[i][j] = ((T)rand()) / (T)(RAND_MAX)*thresh;
                    }
                }
            }

            /**
             * @brief Set the small value to zero
             * 
             * @param thresh the threshold of small value 
             */
            void set_zero(T thresh = 1e-8)
            {
                for (int i = 0; i < this->h; i++)
                {
                    for (int j = 0; j < this->w; j++)
                    {
                        if (DL_ABS(this->array[i][j]) < thresh)
                        {
                            this->array[i][j] = 0;
                        }
                    }
                }
            }

            /**
             * @brief Set the matrix value from a vector
             * 
             * @tparam TT 
             * @param mat   the input vector
             */
            template <typename TT>
            void set_value(std::vector<TT> mat)
            {
                int area = this->w * this->h;
                assert(area == mat.size());
                int index = 0;
                for (int i = 0; i < this->h; i++)
                {
                    for (int j = 0; j < this->w; j++)
                    {
                        this->array[i][j] = (T)(mat[index++]);
                    }
                }
            }

            /**
             * @brief Set the matrix value from another matrix.
             * 
             * @tparam TT 
             * @param mat the input matrix.
             */
            template <typename TT>
            void set_value(const Matrix<TT> &mat)
            {
                assert((this->h == mat.h) && (this->w == mat.w));
                for (int i = 0; i < this->h; i++)
                {
                    for (int j = 0; j < this->w; j++)
                    {
                        this->array[i][j] = (T)(mat.array[i][j]);
                    }
                }
            }

            /**
             * @brief Set a part of the matrix value from another matrix.
             * 
             * @param h_start    the start index of height
             * @param h_end      the end index of height
             * @param w_start    the start index of width
             * @param w_end      the end index of width
             * @param mat        the input matrix
             */
            void set_value(int h_start, int h_end, int w_start, int w_end, const Matrix<T> &mat)
            {
                int h = h_end - h_start;
                int w = w_end - w_start;

                assert((h == mat.h) && (w == mat.w));
                assert((h_end <= this->h) && (w_end <= this->w) && (h_start >= 0) && (w_start >= 0));
                for (int i = 0; i < h; i++)
                {
                    for (int j = 0; j < w; j++)
                    {
                        this->array[i + h_start][j + w_start] = mat.array[i][j];
                    }
                }
            }

            /**
             * @brief Set the matrix value to a constant.
             * 
             * @tparam TT 
             * @param s  the input value.
             */
            template <typename TT>
            void set_value(TT s)
            {
                for (int i = 0; i < this->h; i++)
                {
                    for (int j = 0; j < this->w; j++)
                    {
                        this->array[i][j] = (T)s;
                    }
                }
            }

            /**
             * @brief print the matrix element.
             * 
             */
            void print_value() const
            {
                printf("h: %d, w: %d\n", this->h, this->w);
                for (int i = 0; i < this->h; i++)
                {
                    for (int j = 0; j < this->w; j++)
                    {
                        printf("%f ", (float)(this->array[i][j]));
                    }
                    printf("\n");
                }
            }

            /**
             * @brief  do matrix multiply
             * 
             * @param input     the input matrix
             * @return Matrix<T> the output matrix
             */
            Matrix<T> matmul(const Matrix<T> &input) const;

            /**
             * @brief transpose the matrix
             * 
             * @return Matrix<T> the transposed matrix
             */
            Matrix<T> transpose() const;

            /**
             * @brief get the inverse matrix
             * 
             * @return Matrix<T> the output matrix
             */
            Matrix<T> inverse() const;

            /**
             * @brief get the diagonal of the matrix
             * 
             * @return Matrix<T> the diagonal
             */
            Matrix<T> diagonal() const;

            /**
             * @brief slice the matrix
             * 
             * @param h_start   the start index of height
             * @param h_end     the end index of height
             * @param w_start   the start index of width
             * @param w_end     the end index of width
             * @return Matrix<T> the output.
             */
            Matrix<T> slice(int h_start, int h_end, int w_start, int w_end) const;

            /**
             * @brief get an identity matrix
             * 
             * @param n     the dim of the identity matrix
             * @return Matrix<T>  the output
             */
            static Matrix<T> identity(int n)
            {
                Matrix<T> A(n, n);
                for (int i = 0; i < n; ++i)
                {
                    A.array[i][i] = 1;
                }
                return A;
            }

            /**
             * @brief get a diag matrix
             * 
             * @param d  the diagonal value.
             * @return Matrix<T>  the output
             */
            static Matrix<T> diag(const Matrix<T> &d)
            {
                assert(d.h == 1);
                Matrix<T> A(d.w, d.w);
                for (int i = 0; i < d.w; ++i)
                {
                    A.array[i][i] = d.array[0][i];
                }
                return A;
            }


            static Matrix<T> arange(uint32_t n)
            {
                Matrix<T> A(1, n);
                for (int i = 0; i < n; ++i)
                {
                    A.array[0][i] = i;
                }
                return A;
            }

            static Matrix<T> arange(uint32_t n1, uint32_t n2)
            {
                int len = n2 - n1;
                assert(len > 0);
                Matrix<T> A(1, len);
                for (int i = 0; i < len; ++i)
                {
                    A.array[0][i] = n1 + i;
                }

                return A;
            }

            /**
             * @brief get the F_norm of the matrix
             * 
             * @return T the output F_norm
             */
            T F_norm() const
            {
                T f_n = 0.0;
                for (int i = 0; i < this->h; ++i)
                {
                    for (int j = 0; j < this->w; ++j)
                    {
                        f_n += (this->array[i][j] * this->array[i][j]);
                    }
                }
                f_n = sqrt_newton(f_n);
                return f_n;
            }

            Matrix<T> &operator=(const Matrix<T> &A)
            {
                if ((A.h == this->h) && (A.w == this->w))
                {
                    for (int i = 0; i < A.h; ++i)
                    {
                        for (int j = 0; j < A.w; ++j)
                        {
                            this->array[i][j] = A.array[i][j];
                        }
                    }
                }
                else
                {
                    if (this->array != NULL)
                    {
                        for (int i = 0; i < this->h; ++i)
                        {
                            free(this->array[i]);
                        }
                        free(this->array);
                        this->array = NULL;
                    }
                    this->h = A.h;
                    this->w = A.w;
                    if ((A.h > 0) && (A.w > 0))
                    {
                        this->calloc_element();
                        this->set_value(A);
                    }
                }
                return *this;
            }
        };

        /**
         * @brief Get the affine transform matrix
         * 
         * @param source_coord the source coordinates
         * @param dest_coord   the target coordinates
         * @return Matrix<float> the output matrix
         */
        Matrix<float> get_affine_transform(Matrix<float> &source_coord, Matrix<float> &dest_coord);
        
        /**
         * @brief Get the similarity transform matrix
         * 
         * @param source_coord the source coordinates
         * @param dest_coord the target coordinates
         * @return Matrix<float> the output matrix
         */
        Matrix<float> get_similarity_transform(Matrix<float> &source_coord, Matrix<float> &dest_coord);
        
        /**
         * @brief Get the perspective transform matrix
         * 
         * @param source_coord the source coordinates
         * @param dest_coord   the target coordinates
         * @return Matrix<float> the output matrix
         */
        Matrix<float> get_perspective_transform(Matrix<float> &source_coord, Matrix<float> &dest_coord);
    } // namespace math
} // namespace dl