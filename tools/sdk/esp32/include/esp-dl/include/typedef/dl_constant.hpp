#pragma once

#include "dl_define.hpp"
#include <vector>
#include <stdint.h>

namespace dl
{
    /**
     * @brief Base class of Filter, Bias, Activation.
     * 
     * @tparam T supports int16_t and int8_t,
     *         - int16_t: stands for operation in int16_t quantize,
     *         - int8_t: stands for operation in int8_t quantize.
     */
    template <typename T>
    class Constant
    {
    public:
        const T *element;             /*<! point to element. >*/
        const int exponent;           /*<! exponent of element. >*/
        const std::vector<int> shape; /*<! shape of element. >*/

        /**
         * @brief Construct a new Constant object.
         * 
         * @param element  point to element.
         * @param exponent exponent of element.
         * @param shape    shape of Constant.
         */
        Constant(const T *element, const int exponent, const std::vector<int> shape);
    };

    /**
     * @brief Filter.
     * NOTE: The shape format of filter is fixed, but the element sequence depands on optimization method.
     *       - 1D: reserved
     *       - 2D: shape format is [filter_height, filter_width, input_channel, output_channel]. dilation format is [height, width]
     *  
     * @tparam T supports int16_t and int8_t,
     *         - int16_t: stands for operation in int16_t quantize,
     *         - int8_t: stands for operation in int8_t quantize.
     */
    template <typename T>
    class Filter : public Constant<T>
    {
    public:
        const std::vector<int> dilation;      /*<! - 1D: reserved >*/
                                              /*<! - 2D: [dilation_in_height, dilation_in_width] >*/
        std::vector<int> shape_with_dilation; /*<! - 1D: reserved >*/
                                              /*<! - 2D: [filter_height_with_dilation, filter_width_with_dilation, input_channel, output_channel] >*/
        const int8_t* channel_exponent;       /*<! exponent for per-channel >*/
        const int channel_exponent_size;

        /**
         * @brief Construct a new Filter object. 
         * 
         * @param element  point to element
         * @param exponent exponent of element
         * @param shape    shape of Filter,
         *                 - 1D: reserved
         *                 - 2D: for convolution is [filter_height, filter_width, input_channel, output_channel],
         *                       for depthwise convolution is [filter_height, filter_width, input_channel, 1]
         * @param dilation dilation of Filter
         *                 - 1D: reserved
         *                 - 2D: [dilation_in_height, dilation_in_width]
         */
        Filter(const T *element, const int exponent, const std::vector<int> shape, const std::vector<int> dilation = {1, 1});

        /**
         * @brief Construct a new Filter object. it is only avaliable to int16_t
         * 
         * @param element               point to element
         * @param channel_exponent      exponent for per-channel
         * @param channel_exponent_size size of exponent
         * @param shape                 shape of element
         * @param dilation              dilation of Filter
         *                              - 1D: reserved
         *                              - 2D: [dilation_in_height, dilation_in_width]
         */
        Filter(const T *element, const int8_t* channel_exponent, const int channel_exponent_size, const std::vector<int> shape, const std::vector<int> dilation = {1, 1});

        /**
         * @brief Print the n-th filter.
         * 
         * @param n       index of output_channel
         * @param message to print
         */
        void print2d_n(const int n, const char *message) const;
    };

    /**
     * @brief Bias.
     * 
     * @tparam T supports int16_t and int8_t
     *         - int16_t: stands for operation in int16_t quantize
     *         - int8_t: stands for operation in int8_t quantize
     */
    template <typename T>
    class Bias : public Constant<T>
    {
    public:
        using Constant<T>::Constant;
    };

    /**
     * @brief Activation.
     * 
     * @tparam T supports int16_t and int8_t
     *         - int16_t: stands for operation in int16_t quantize
     *         - int8_t: stands for operation in int8_t quantize
     */
    template <typename T>
    class Activation : public Constant<T>
    {
    public:
        const activation_type_t type; /*<! One of Linear or ReLU or LeakyReLU or PReLU */

        /**
         * @brief Construct a new Activation object.
         * 
         * @param type      One of Linear or ReLU or LeakyReLU or PReLU
         * @param element   point to element of activation
         * @param exponent  exponent of element
         * @param shape     shape of element
         */
        Activation(const activation_type_t type, const T *element = NULL, const int exponent = 0, const std::vector<int> shape = {0});
    };
} // namespace dl