#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_tool.hpp"
#include "dl_nn_relu.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief ReLU(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class Relu : public Layer
        {
        private:
            Tensor<feature_t> *output;     /*<! output ptr of relu >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                false: the output will store to a separate memory >*/
            std::vector<int> output_shape; /*<! output shape of relu >*/
        public:
            /**
             * @brief Construct a new ReLU object
             * 
             * @param name            name of relu
             * @param inplace         true: the output will store to input0
             *                        false: the output will store to a separate memory
             */
            Relu(const char *name = "Relu", bool inplace = false) : Layer(name),
                                                                    output(NULL), inplace(inplace), output_shape({})
            {
            }

            /**
             * @brief Destroy the ReLU object
             * 
             */
            ~Relu()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
             * @brief Update output shape and exponent
             * 
             * @param input       as an input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                this->output_shape = input.shape;
                if (!this->inplace)
                {
                    if (this->output == NULL)
                    {
                        this->output = new Tensor<feature_t>;
                    }
                    this->output->set_exponent(input.exponent);
                    this->output->set_shape(this->output_shape);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input;
                }

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& ReLU result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call ReLU operation.
             * 
             * @param input       as an input
             * @param assign_core not effective yet
             * @return ReLU result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input, const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if (!this->inplace)
                {
                    DL_LOG_LAYER_LATENCY_START();
                    if (this->output->shape != this->output_shape)
                    {
                        this->output->set_shape(this->output_shape);
                    }
                    this->output->malloc_element();
                    this->output->set_exponent(input.exponent);
                    DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                    DL_LOG_LAYER_LATENCY_START();
                    nn::relu(*this->output, input, assign_core);
                    DL_LOG_LAYER_LATENCY_END(this->name, "relu");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    if (this->output->shape != this->output_shape)
                    {
                        this->output->set_shape(this->output_shape);
                    }
                    nn::relu(*this->output, input, assign_core);
                    DL_LOG_LAYER_LATENCY_END(this->name, "relu");
                }

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
