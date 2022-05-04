#pragma once

#include "dl_nn_pad.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Pad.
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class Pad : public Layer
        {
        private:
            std::vector<int> paddings;
            std::vector<feature_t> constant_values;
            padding_mode_t mode;
            Tensor<feature_t> *output;     /*<! output ptr of Pad >*/
            std::vector<int> output_shape; /*<! output shape of Pad >*/

        public:
            Pad(std::vector<int> paddings,
                std::vector<feature_t> constant_values = {0},
                padding_mode_t mode = PADDING_CONSTANT,
                const char *name = "Pad") : Layer(name),
                                            paddings(paddings),
                                            constant_values(constant_values),
                                            mode(mode)
            {
                this->output = new Tensor<feature_t>;
            }

            /**
             * @brief Destroy the Pad object.
             * 
             */
            ~Pad()
            {
                if (this->output != NULL)
                {
                    delete this->output;
                }
            }

            /**
             * @brief Update output padding and input padding.
             * 
             * @param input as an input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                assert(this->paddings.size() > 0);
                int input_dims = input.shape.size();
                int padding_dims = input_dims * 2;
                if (this->paddings.size() == 1)
                {
                    std::vector<int> _paddings(padding_dims, 0);
                    for (int i = 0; i < padding_dims; ++i)
                    {
                        _paddings[i] = this->paddings[0];
                    }
                    this->paddings = _paddings;
                }
                else if (this->paddings.size() == 2)
                {
                    std::vector<int> _paddings(padding_dims, 0);
                    for (int i = 0; i < input_dims; ++i)
                    {
                        _paddings[2 * i] = this->paddings[0];
                        _paddings[2 * i + 1] = this->paddings[1];
                    }
                    this->paddings = _paddings;
                }
                else
                {
                    assert(this->paddings.size() == padding_dims);
                }

                if (this->mode == PADDING_CONSTANT)
                {
                    if (this->constant_values.size() == 1)
                    {
                        std::vector<feature_t> _constant_values(padding_dims, 0);
                        for (int i = 0; i < padding_dims; ++i)
                        {
                            _constant_values[i] = this->constant_values[0];
                        }
                        this->constant_values = _constant_values;
                    }
                    else if (this->constant_values.size() == 2)
                    {
                        std::vector<feature_t> _constant_values(padding_dims, 0);
                        for (int i = 0; i < input_dims; ++i)
                        {
                            _constant_values[2 * i] = this->constant_values[0];
                            _constant_values[2 * i + 1] = this->constant_values[1];
                        }
                        this->constant_values = _constant_values;
                    }
                    else
                    {
                        assert(constant_values.size() == padding_dims);
                    }
                }
                this->output_shape = input.shape;
                for (int i = 0; i < input_dims; ++i)
                {
                    this->output_shape[i] += (this->paddings[2 * i] + this->paddings[2 * i + 1]);
                }

                this->output->set_shape(this->output_shape);
                this->output->set_exponent(input.exponent);
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
             * @return Tensor<feature_t>& Pad result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Pad operation
             * 
             * @param input           as an input.
             * @param autoload_enable one of true or false, 
             *                        - true: load input and output from PSRAM to CACHE automatically
             *                        - false: do not
             * @param assign_core     not effective yet
             * @return Pad result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input, const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
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

                DL_LOG_LAYER_LATENCY_START();
                nn::pad(*this->output, input, this->paddings, this->constant_values, this->mode, assign_core);
                DL_LOG_LAYER_LATENCY_END(this->name, "pad");
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
