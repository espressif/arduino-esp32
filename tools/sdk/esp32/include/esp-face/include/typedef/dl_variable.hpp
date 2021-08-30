#pragma once

#include <stdio.h>
#include <vector>
#include <assert.h>

#include "dl_tool.hpp"

namespace dl
{
    /**
     * @brief Tensor
     * 
     * @tparam T support uint8_t, int8_t, int16_t and float.
     */
    template <typename T>
    class Tensor
    {
    private:
        int size;       /*<! size of element including padding */
        bool auto_free; /*<! free element when object destroy */

    public:
        T *element;                          /*<! point to element */
        int exponent;                        /*<! exponent of element */
        std::vector<int> shape;              /*<! shape of Tensor */
                                             /*<! 2D: shape is [height, width, channel] */
                                             /*<! 1D: reserved */
        std::vector<int> shape_with_padding; /*<! shape with padding of Tensor */
                                             /*<! 2D: shape_with_padding is [height_with_padding, width_with_padding, channel_with_padding] */
                                             /*<! 1D: reserved */
        std::vector<int> padding;            /*<! padding of Tensor */
                                             /*<!- 2D: padding format is [top, bottom, left, right] */
                                             /*<! - 1D: reserved */

        /**
         * @brief Construct a new Tensor object
         * 
         */
        Tensor() : size(-1), auto_free(true), element(NULL), exponent(0) {}

        /**
         * @brief Construct a new Tensor object by copying from input.
         * 
         * @param input an input Tensor
         * @param deep  one of true or false
         *              - true: apply a new memory, copy value from input.element to this new memory
         *              - false: take over input.element to this->element
         */
        Tensor(Tensor<T> &input, bool deep) : size(input.size),
                                              auto_free(input.auto_free),
                                              exponent(input.exponent),
                                              shape(input.shape),
                                              shape_with_padding(input.shape_with_padding),
                                              padding(input.padding)
        {
            if (deep)
            {
                int size_real = input.shape_with_padding.size() ? input.shape_with_padding[0] * input.shape_with_padding[1] * input.shape_with_padding[2] : 0;
                T *new_element = (T *)tool::calloc_aligned(size_real, sizeof(T), 16);
                tool::copy_memory(new_element, input.element, size_real * sizeof(T));
                this->element = new_element;
            }
            else
            {
                this->element = input.element;
            }
        }

        /**
         * @brief Destroy the Tensor object
         * 
         */
        ~Tensor()
        {
            if (this->auto_free)
                this->free_element();
        }

        /**
         * @brief Set the auto free object.
         * 
         * @param auto_free one of true or false
         *                  - true: free element when object destroyed
         *                  - false: do not
         * @return self
         */
        Tensor<T> &set_auto_free(const bool auto_free)
        {
            this->auto_free = auto_free;
            return *this;
        }

        /**
         * @brief Set the element.
         * 
         * @param element point to element memory
         * @return self
         */
        Tensor<T> &set_element(T *element, const bool auto_free = false)
        {
            assert(this->element == NULL);
            this->element = element;
            this->auto_free = auto_free;

            return *this;
        }

        /**
         * @brief Set the exponent.
         * 
         * @param exponent exponent of element
         * @return self
         */
        Tensor<T> &set_exponent(const int exponent)
        {
            this->exponent = exponent;

            return *this;
        }

        /**
         * @brief Set the shape of Tensor. Initial this->padding = {0}. Initial this->size = -1.
         * 
         * @param shape shape in 
         *              - 2D: [height, width]
         * @return self
         */
        Tensor<T> &set_shape(const std::vector<int> shape)
        {
            for (int i = 0; i < shape.size(); ++i)
            {
                assert(shape[i] > 0);
            }
            this->shape = shape;
            this->shape_with_padding = shape;
            this->size = -1;
            this->padding = std::vector<int>(((this->shape.size() - 1) << 1), 0);
            return *this;
        }

        /**
         * @brief Set the padding size object.
         * 
         * @param padding padding size in
         *                - 2D: [top, bottom, left, right]
         * @return self
         */
        Tensor &set_padding_size(std::vector<int> &padding)
        {
            assert(this->shape.size());      // call Tensor.set_shape() first
            assert(this->shape.size() == 3); // TODO: || this->shape.size() == 2

            if (this->shape.size() == 3)
            {
                std::vector<int> new_padding = this->padding;
                bool dont_update = true;

                if (padding[0] > this->padding[0])
                {
                    new_padding[0] = padding[0];
                    dont_update = false;
                }

                if (padding[1] > this->padding[1])
                {
                    new_padding[1] = padding[1];
                    dont_update = false;
                }

                if (padding[2] > this->padding[2])
                {
                    new_padding[2] = padding[2];
                    dont_update = false;
                }

                if (padding[3] > this->padding[3])
                {
                    new_padding[3] = padding[3];
                    dont_update = false;
                }

                if (dont_update)
                {
                    return *this;
                }

                std::vector<int> new_shape_with_padding = this->shape;

                new_shape_with_padding[0] += (new_padding[0] + new_padding[1]);
                new_shape_with_padding[1] += (new_padding[2] + new_padding[3]);
                int new_size = new_shape_with_padding[0] * new_shape_with_padding[1] * new_shape_with_padding[2];

                if (this->element) // if this->element != NULL, do padding by copy memory
                {
                    T *new_element = (T *)tool::malloc_aligned(new_size, sizeof(T), 16);
                    T *dst = new_element + ((new_padding[0] * new_shape_with_padding[1]) + new_padding[2]) * new_shape_with_padding[2];
                    T *src = this->get_element_ptr();
                    int offset_dst_next_y = new_shape_with_padding[1] * new_shape_with_padding[2];     // width * channel
                    int src_copy_length = this->shape[1] * this->shape[2];                             // width * channel
                    int offset_src_next_y = this->shape_with_padding[1] * this->shape_with_padding[2]; // width * channel
                    for (int y = 0; y < this->shape[0]; y++)
                    {
                        tool::copy_memory(dst, src, src_copy_length * sizeof(T));
                        dst += offset_dst_next_y;
                        src += offset_src_next_y;
                    }

                    if (this->auto_free)
                        tool::free_aligned(this->element);
                    this->element = new_element;
                    this->auto_free = true;
                }
                this->padding = new_padding;
                this->shape_with_padding = new_shape_with_padding;
                this->size = new_size;
            }
            else if (this->shape.size() == 2)
            {
                printf("Tensor.set_padding_size with this->shape.size() == 2 not implement yet.\n");
            }

            return *this;
        }

        /**
         * @brief Set the padding value object.
         * 
         * @param padding padding size in
         *                - 2D: [top, bottom, left, right]
         * @param value   value to set
         * @return self
         */
        Tensor<T> &set_padding_value(std::vector<int> &padding, T value);

        /**
         * @brief Get the element pointer.
         * 
         * @param padding padding size in
         *                - 2D: [top, bottom, left, right]
         * @return pointer to memory with padding
         */
        T *get_element_ptr(const std::vector<int> padding = {0, 0, 0, 0})
        {
            assert(this->shape.size() == 3); // TODO: || this->shape.size() == 2

            if (this->shape.size() == 3)
            {
                return this->element + ((this->padding[0] - padding[0]) * this->shape_with_padding[1] + (this->padding[2] - padding[2])) * this->shape_with_padding[2];
            }
            else if (this->shape.size() == 2)
            {
                printf("Tensor.get_element_ptr with this->shape.size() == 2 is not implemented.\n");
            }

            return NULL;
        }

        /**
         * @brief Get the element value.
         * 
         * @param index        index in
         *                     - 2D: [y, x, c]
         * @param with_padding one of true or false,
         *                     - true: make padding size in count
         *                     - false: do not
         * @return element value
         */
        T &get_element_value(const std::vector<int> index, const bool with_padding = false)
        {
            assert(index.size() == this->shape.size());
            assert(this->shape.size() == 3); // TODO: || this->shape() == 2

            int i = 0;
            if (this->shape.size() == 3)
            {
                int y = index[0];
                int x = index[1];
                int c = index[2];
                i = with_padding ? (y * this->shape_with_padding[1] + x) * this->shape_with_padding[2] + c : ((y + this->padding[0]) * this->shape_with_padding[1] + x + this->padding[2]) * this->shape_with_padding[2] + c;
            }
            else if (this->shape.size() == 2)
            {
                printf("Tensor.get_element_value with this->shape.size() == 2 is not implemented.\n");
            }

            return this->element[i];
        }

        /**
         * @brief Get the size of element.
         * 
         * @return size of element including padding
         */
        int get_size()
        {
            if (this->size == -1) // didn't call Tensor.set_padding_size() before
            {
                this->size = 1;
                for (std::vector<int>::iterator d = this->shape.begin(); d != this->shape.end(); d++)
                    this->size *= *d;
            }

            return this->size;
        }

        /**
         * @brief Apply memory with zero-initialized only if this->element is NULL.
         * 
         * @param auto_free one of true or false
         *                  - true: free element when object destroyed
         *                  - false: do not
         * @return 
         *         - true: on success
         *         - false: if applying failed
         */
        bool calloc_element(const bool auto_free = true)
        {
            if (this->element != NULL)
                return false;

            this->element = (T *)dl::tool::calloc_aligned(this->get_size(), sizeof(T), 16);
            this->auto_free = auto_free;

            return true;
        }

        /**
         * @brief Apply memory without initialized only if this->element is NULL.
         * 
         * @param auto_free one of true or false
         *                  - true: free element when object destroyed
         *                  - false: do not
         * @return 
         *         - true: on success
         *         - false: if applying failed
         */
        bool malloc_element(const bool auto_free = true)
        {
            if (this->element != NULL)
                return false;

            this->element = (T *)tool::malloc_aligned(this->get_size(), sizeof(T), 16);
            this->auto_free = auto_free;

            return true;
        }

        /**
         * @brief If this->element != NULL no memory will be applied and no value will be set in padding.
         * Else apply memory without initialized and set value to padding.
         * 
         * @param padding_value value to set in padding
         * @param auto_free     one of true of false
         *                      - true: free element when object destroyed
         *                      - false: do not
         * @return 
         *         - true: apply memory and set padding value successfully
         *         - false: no memory applied and no padding value set
         */
        bool apply_element(const T padding_value = 0, const bool auto_free = true)
        {
            if (this->element != NULL)
                return false;

            this->element = (T *)tool::malloc_aligned(this->get_size(), sizeof(T), 16);
            this->set_padding_value(this->padding, padding_value);
            this->auto_free = auto_free;

            return true;
        }

        /**
         * @brief free element only if this->element != NULL
         * set this->element to NULL, after free
         * @brief Free element if this->element is not NULL.
         */
        void free_element()
        {
            if (this->auto_free && this->element)
            {
                tool::free_aligned(this->element);
                this->element = NULL;
            }
        }

        /**
         * @brief Print the shape of Tensor in format "shape = ({top_padding} + {height} + {bottom_padding}, {left_padding} + {width} + {right_padding}, {channel}(channel_with_padding))\n".
         */
        void print_shape()
        {
            printf("shape = (%d + %d + %d, %d + %d + %d, %d(%d))\n",
                   this->padding[0], this->shape[0], this->padding[1],
                   this->padding[2], this->shape[1], this->padding[3],
                   this->shape[2], this->shape_with_padding[2]);
        }

        /**
         * @brief Take numpy for example, this function print Tensor[y_start:y_end, x_start:x_end, c_start:c_end].
         * 
         * inner box is effective value of Tensor, "0" around is padding.
         * 
         * (with padding)
         *               00000000000000000000000000000000000000000000000000
         *               00000000000000000000000000000000000000000000000000
         *               00000000000000000000000000000000000000000000000000
         *               000000(without padding)                   00000000
         *               000000                                    00000000
         *               000000                                    00000000
         *               000000          effective value           00000000
         *               000000                                    00000000
         *               000000                                    00000000
         *               00000000000000000000000000000000000000000000000000
         *               00000000000000000000000000000000000000000000000000
         *               00000000000000000000000000000000000000000000000000
         * 
         * @param y_start start index in height
         * @param y_end   end index in height
         * @param x_start start index in width
         * @param x_end   end index in width
         * @param c_start start index in channel
         * @param c_end   end index in channel
         * @param message to print
         * @param axis    print aligned this axis, effective only if all y_end - y_start, x_end - x_start and c_end - c_start equals to 1
         * @param with_padding one of true or false,
         *                     - true: count from (with padding) in upper image
         *                     - false: count from (without padding) in upper image
         */
        void print(int y_start, int y_end,
                   int x_start, int x_end,
                   int c_start, int c_end,
                   const char *message, int axis = 0, const bool with_padding = false)
        {
            assert(y_end > y_start);
            assert(x_end > x_start);
            assert(c_end > c_start);

            y_start = DL_MAX(y_start, 0);
            x_start = DL_MAX(x_start, 0);
            c_start = DL_MAX(c_start, 0);
            if (with_padding)
            {
                y_end = DL_MIN(y_end, this->shape_with_padding[0]);
                x_end = DL_MIN(x_end, this->shape_with_padding[1]);
                c_end = DL_MIN(c_end, this->shape_with_padding[2]);
            }
            else
            {
                y_end = DL_MIN(y_end, this->shape[0]);
                x_end = DL_MIN(x_end, this->shape[1]);
                c_end = DL_MIN(c_end, this->shape[2]);
            }

            printf("%s[%d:%d, %d:%d, %d:%d] | ", message, y_start, y_end, x_start, x_end, c_start, c_end);
            this->print_shape();

            if (y_end - y_start == 1)
            {
                if (x_end - x_start == 1)
                {
                    for (int c = c_start; c < c_end; c++)
                        printf("%7d", c);
                    printf("\n");

                    for (int c = c_start; c < c_end; c++)
                        printf("%7d", this->get_element_value({y_start, x_start, c}, with_padding));
                    printf("\n");

                    return;
                }
                else
                {
                    if (c_end - c_start == 1)
                    {
                        for (int x = x_start; x < x_end; x++)
                            printf("%7d", x);
                        printf("\n");

                        for (int x = x_start; x < x_end; x++)
                            printf("%7d", this->get_element_value({y_start, x, c_start}, with_padding));
                        printf("\n");

                        return;
                    }
                }
            }
            else
            {
                if (x_end - x_start == 1)
                {
                    if (c_end - c_start == 1)
                    {
                        for (int y = y_start; y < y_end; y++)
                            printf("%7d", y);
                        printf("\n");

                        for (int y = y_start; y < y_end; y++)
                            printf("%7d", this->get_element_value({y, x_start, c_start}, with_padding));
                        printf("\n");

                        return;
                    }
                }
            }

            if (y_end - y_start == 1)
                axis = 0;

            if (x_end - x_start == 1)
                axis = 1;

            if (c_end - c_start == 1)
                axis = 2;

            if (axis == 0)
            {
                // ______c
                // |
                // |
                // x
                //
                for (int y = y_start; y < y_end; y++)
                {
                    printf("y = %d\n     ", y);

                    for (int c = c_start; c < c_end; c++)
                        printf("%7d", c);
                    printf("\n");

                    for (int x = x_start; x < x_end; x++)
                    {
                        printf("%5d", x);
                        for (int c = c_start; c < c_end; c++)
                            printf("%7d", this->get_element_value({y, x, c}, with_padding));
                        printf("\n");
                    }
                    printf("\n");
                }
            }
            else if (axis == 1)
            {
                // ______c
                // |
                // |
                // y
                //
                for (int x = x_start; x < x_end; x++)
                {
                    printf("x = %d\n     ", x);

                    for (int c = c_start; c < c_end; c++)
                        printf("%7d", c);
                    printf("\n");

                    for (int y = y_start; y < y_end; y++)
                    {
                        printf("%5d", y);
                        for (int c = c_start; c < c_end; c++)
                            printf("%7d", this->get_element_value({y, x, c}, with_padding));
                        printf("\n");
                    }
                    printf("\n");
                }
            }
            else
            {
                // ______x
                // |
                // |
                // y
                //
                for (int c = c_start; c < c_end; c++)
                {
                    printf("c = %d\n     ", c);

                    for (int x = x_start; x < x_end; x++)
                        printf("%7d", x);
                    printf("\n");

                    for (int y = y_start; y < y_end; y++)
                    {
                        printf("%5d", y);
                        for (int x = x_start; x < x_end; x++)
                            printf("%7d", this->get_element_value({y, x, c}, with_padding));
                        printf("\n");
                    }
                    printf("\n");
                }
            }

            return;
        }

        /**
         * @brief print all the element of the Tensor.
         * 
         * @param message to print
         * @param with_padding one of true or false,
         *                     - true: the padding element will also be printed
         *                     - false: the padding element will not be printed
         */
        void print_all(const char *message, const bool with_padding = false)
        {
            int y_end;
            int x_end;
            int c_end;
            if (with_padding)
            {
                y_end = this->shape_with_padding[0];
                x_end = this->shape_with_padding[1];
                c_end = this->shape_with_padding[2];
            }
            else
            {
                y_end = this->shape[0];
                x_end = this->shape[1];
                c_end = this->shape[2];
            }

            printf("\n%s | ", message);
            this->print_shape();

            for (int y = 0; y < y_end; y++)
            {
                for (int x = 0; x < x_end; x++)
                {
                    for (int c = 0; c < c_end; c++)
                        printf("%d ", this->get_element_value({y, x, c}, with_padding));
                }
            }
            printf("\n");
            return;
        }

        /**
         * @brief Check the element value with input ground-truth.
         * 
         * @param gt_element ground-truth value of element
         * @param bias permissible error
         * @param info one of true or false
         *             - true: print shape and result
         *             - false: do not
         * @return 
         *         - true: in permissible error
         *         - false: not 
         */
        bool check_element(T *gt_element, int bias = 2, bool info = true)
        {
            if (info)
                this->print_shape();
            int i = 0;
            for (int y = 0; y < this->shape[0]; y++)
            {
                for (int x = 0; x < this->shape[1]; x++)
                {
                    for (int c = 0; c < this->shape[2]; c++)
                    {
                        int a = this->get_element_value({y, x, c});
                        int b = gt_element[i];
                        int offset = DL_ABS(a - b);
                        if (offset > bias)
                        {
                            printf("element[%d, %d, %d]: %d v.s. %d\n", y, x, c, a, b);
                            return false;
                        }
                        i++;
                    }
                }
            }

            if (info)
                printf("PASS\n");

            return true;
        }

        /**
         * @brief Check the shape is the same as the shape of input.
         * 
         * @param input an input tensor 
         * @return 
         *         - true: same shape 
         *         - false: not 
         */
        bool is_same_shape(Tensor<T> &input)
        {
            if (input.shape.size() != this->shape.size())
            {
                return false;
            }
            for (int i = 0; i < this->shape.size(); i++)
            {
                if (input.shape[i] != this->shape[i])
                {
                    return false;
                }
            }
            return true;
        }

        Tensor<T> &operator=(const Tensor<T> &input)
        {
            this->size = input.size;
            this->auto_free = input.auto_free;
            this->exponent = input.exponent;
            this->shape = input.shape;
            this->padding = input.padding;
            int size_real_tmp = this->shape_with_padding.size() ? this->shape_with_padding[0] * this->shape_with_padding[1] * this->shape_with_padding[2] : 0;
            int size_input_real = input.shape_with_padding.size() ? input.shape_with_padding[0] * input.shape_with_padding[1] * input.shape_with_padding[2] : 0;
            this->shape_with_padding = input.shape_with_padding;
            if (this->element)
            {
                if (size_real_tmp != size_input_real)
                {
                    tool::free_aligned(this->element);
                    T *new_element = (T *)tool::calloc_aligned(size_input_real, sizeof(T), 16);
                    tool::copy_memory(new_element, input.element, size_input_real * sizeof(T));
                    this->element = new_element;
                }
                else
                {
                    tool::copy_memory(this->element, input.element, size_input_real * sizeof(T));
                }
            }
            else
            {
                T *new_element = (T *)tool::calloc_aligned(size_input_real, sizeof(T), 16);
                tool::copy_memory(new_element, input.element, size_input_real * sizeof(T));
                this->element = new_element;
            }
            return *this;
        }
    };
} // namespace dl