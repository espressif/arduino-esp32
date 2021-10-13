#pragma once

#include <vector>
#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn_max_pool2d.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief MaxPool2D(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class MaxPool2D : public Layer
        {
        private:
            std::vector<int> filter_shape;     /*<! filter shape in [filter_height, filter_width] >*/
            const int stride_y;                /*<! stride in height >*/
            const int stride_x;                /*<! stride in width >*/
            const padding_type_t padding_type; /*<! one of PADDING_VALID or PADDING_SAME or PADDING_SAME_MXNET >*/
            std::vector<int> padding;          /*<! padding size needed in [top, bottom, left, right] of this operation >*/
            Tensor<feature_t> *output;         /*<! output ptr of MaxPool2D >*/

        public:

            /**
             * @brief Construct a new MaxPool2D object.
             * 
             * @param filter_shape filter shape in [filter_height, filter_width]
             * @param padding_type one of PADDING_VALID or PADDING_SAME or PADDING_SAME_MXNET,
             *                     - PADDING_VALID means no padding
             *                     PADDING_SAME and PADDING_SAME_MXNET results in padding with zeros evenly to the left/right or up/down of the input 
             *                     such that output has the same height/width dimension as the input,
             *                     - PADDING_SAME results padding in TensorFlow style
             *                     - PADDING_SAME_MXNET results padding in MXNET style
             * @param stride_y     stride in height
             * @param stride_x     stride in width
             * @param name         name of layer
             */
            MaxPool2D(const std::vector<int> filter_shape,
                      const padding_type_t padding_type = PADDING_VALID,
                      const int stride_y = 1,
                      const int stride_x = 1,
                      const char *name = NULL) : Layer(name),
                                                 filter_shape(filter_shape),
                                                 stride_y(stride_y),
                                                 stride_x(stride_x),
                                                 padding_type(padding_type)
            {
                this->output = new Tensor<feature_t>;
            }

            /**
             * @brief Destroy the MaxPool2D object.
             * 
             */
            ~MaxPool2D() 
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
             */
            void build(Tensor<feature_t> &input)
            {
                assert(input.shape[0] > 0);
                assert(input.shape[1] > 0);
                this->output->set_exponent(input.exponent);
                std::vector<int> output_shape = nn::get_output_shape(input.shape, filter_shape, this->stride_y, this->stride_x, this->padding_type);
                this->output->set_shape(output_shape);

                this->padding = nn::get_pad_size(output_shape, input.shape, filter_shape, this->stride_y, this->stride_x, this->padding_type);
                input.set_padding_size(this->padding);
                this->output->free_element();
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& MaxPool2D result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call MaxPool2D operation
             * 
             * @param input           as an input
             * @param autoload_enable one of true or false, 
             *                        - true: load input and output from PSRAM to CACHE automatically
             *                        - false: do not
             * @param assign_core     not effective yet
             * @return MaxPool2D result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input, uint8_t autoload_enable = 0)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                DL_LOG_LAYER_LATENCY_START();
                this->output->apply_element();
                this->output->set_exponent(input.exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                if (autoload_enable)
                {
                    dl::tool::cache::autoload_func((uint32_t)(this->output->element), this->output->get_size() * sizeof(feature_t),
                                                   (uint32_t)(input.element), input.get_size() * sizeof(feature_t));
                }

                DL_LOG_LAYER_LATENCY_START();
                nn::max_pool2d(*this->output, input, this->padding, this->filter_shape, this->stride_y, this->stride_x);
                DL_LOG_LAYER_LATENCY_END(this->name, "max_pool2d");

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
