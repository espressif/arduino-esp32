#pragma once

#include <vector>
#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn_avg_pool2d.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief AvgPool2D(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class AvgPool2D : public Layer
        {
        private:
            const int output_exponent;         /*<! exponent of output >*/
            std::vector<int> filter_shape;     /*<! filter shape in [filter_height, filter_width] >*/
            const int stride_y;                /*<! stride in height >*/
            const int stride_x;                /*<! stride in width >*/
            const padding_type_t padding_type; /*<! one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN >*/
            std::vector<int> padding;          /*<! padding size needed in [top, bottom, left, right] of this operation >*/
            Tensor<feature_t> *output;         /*<! output ptr of AvgPool2D >*/
            std::vector<int> output_shape;     /*<! output shape of AvgPool2D >*/

        public:
            /**
             * @brief Construct a new AvgPool2D object.
             * 
             * @param output_exponent exponent of output
             * @param filter_shape    filter shape in [filter_height, filter_width]
             * @param padding_type    one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN or PADDING_NOT_SET,
             *                        - PADDING_VALID means no padding
             *                        PADDING_SAME_END and PADDING_SAME_BEGIN results in padding with zeros evenly to the left/right or up/down of the input 
             *                        such that output has the same height/width dimension as the input,
             *                        - PADDING_SAME_END results padding in TensorFlow style
             *                        - PADDING_SAME_BEGIN results padding in MXNET style
             *                        - PADDING_NOT_SET means padding with the specific "padding" value below. 
             * @param padding         if padding_type is PADDING_NOT_SET, this value will be used as padding size. 
             *                        the shape must be 4, the value of each position is: [padding top, padding bottom, padding left, padding right]
             * @param stride_y        stride in height
             * @param stride_x        stride in width
             * @param name            name of layer
             */
            AvgPool2D(const int output_exponent,
                      const std::vector<int> filter_shape,
                      const padding_type_t padding_type = PADDING_VALID,
                      std::vector<int> padding = {},
                      const int stride_y = 1,
                      const int stride_x = 1,
                      const char *name = "AvgPool2D") : Layer(name),
                                                        output_exponent(output_exponent),
                                                        filter_shape(filter_shape),
                                                        stride_y(stride_y),
                                                        stride_x(stride_x),
                                                        padding_type(padding_type),
                                                        padding(padding),
                                                        output_shape({})
            {
                this->output = new Tensor<feature_t>;
                if (this->padding_type == PADDING_NOT_SET)
                {
                    assert(this->padding.size() == 4);
                }
            }

            /**
            * @brief Destroy the AvgPool2D object.
            * 
            */
            ~AvgPool2D()
            {
                if (this->output != NULL)
                {
                    delete this->output;
                }
            }

            /**
             * @brief Update output shape and padding.
             * 
             * @param input as an input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                assert(input.shape[0] > 0);
                assert(input.shape[1] > 0);
                assert(input.shape.size() == 3);

                this->output_shape = nn::get_output_shape(input.shape, filter_shape, this->stride_y, this->stride_x, this->padding_type, false, this->padding);
                this->output->set_shape(this->output_shape);
                this->output->set_exponent(this->output_exponent);

                if (this->padding_type != PADDING_NOT_SET)
                {
                    this->padding = nn::get_pad_size(this->output_shape, input.shape, filter_shape, this->stride_y, this->stride_x, this->padding_type);
                }

                this->output->free_element();

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& AvgPool2D result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call AvgPool2D operation
             * 
             * @param input           as an input
             * @param autoload_enable one of true or false, 
             *                        - true: load input and output from PSRAM to CACHE automatically
             *                        - false: do not
             * @return AvgPool2D result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input, uint8_t autoload_enable = 0)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                DL_LOG_LAYER_LATENCY_START();
                if (this->output->shape != this->output_shape)
                {
                    this->output->set_shape(this->output_shape);
                }
                this->output->malloc_element();
                this->output->set_exponent(this->output_exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                if (autoload_enable)
                {
                    dl::tool::cache::autoload_func((uint32_t)(this->output->element), this->output->get_size() * sizeof(feature_t),
                                                   (uint32_t)(input.element), input.get_size() * sizeof(feature_t));
                }

                DL_LOG_LAYER_LATENCY_START();
                nn::avg_pool2d(*this->output, input, this->padding, this->filter_shape, this->stride_y, this->stride_x);
                DL_LOG_LAYER_LATENCY_END(this->name, "avg_pool2d");

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
