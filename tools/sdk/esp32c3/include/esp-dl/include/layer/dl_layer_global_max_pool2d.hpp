#pragma once

#include <vector>
#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn_global_max_pool2d.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief GlobalMaxPool2D(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class GlobalMaxPool2D : public Layer
        {
        private:
            Tensor<feature_t> *output;     /*<! output ptr of GlobalMaxPool2D >*/
            std::vector<int> output_shape; /*<! output shape of GlobalMaxPool2D >*/
        public:
            /**
             * @brief Construct a new GlobalMaxPool2D object.
             * 
             * @param name         name of layer
             */
            GlobalMaxPool2D(const char *name = "GlobalMaxPool2D") : Layer(name), output_shape({})
            {
                this->output = new Tensor<feature_t>;
            }

            /**
             * @brief Destroy the GlobalMaxPool2D object.
             * 
             */
            ~GlobalMaxPool2D()
            {
                if (this->output != NULL)
                {
                    delete this->output;
                }
            }

            /**
             * @brief Update output shape and exponent.
             * 
             * @param input as an input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                assert(input.shape[0] > 0);
                assert(input.shape[1] > 0);
                assert(input.shape.size() == 3);
                this->output->set_exponent(input.exponent);

                std::vector<int> output_shape(input.shape.size(), 1);
                output_shape[2] = input.shape[2];
                this->output_shape = output_shape;
                this->output->set_shape(this->output_shape);
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
             * @return Tensor<feature_t>& GlobalMaxPool2D result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call GlobalMaxPool2D operation
             * 
             * @param input           as an input
             * @param autoload_enable one of true or false, 
             *                        - true: load input and output from PSRAM to CACHE automatically
             *                        - false: do not
             * @param assign_core     not effective yet
             * @return GlobalMaxPool2D result
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
                this->output->set_exponent(input.exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                if (autoload_enable)
                {
                    dl::tool::cache::autoload_func((uint32_t)(this->output->element), this->output->get_size() * sizeof(feature_t),
                                                   (uint32_t)(input.element), input.get_size() * sizeof(feature_t));
                }

                DL_LOG_LAYER_LATENCY_START();
                nn::global_max_pool2d(*this->output, input);
                DL_LOG_LAYER_LATENCY_END(this->name, "global_max_pool2d");

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
