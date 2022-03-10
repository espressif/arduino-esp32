#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn_prelu.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief PRelu(input).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class PRelu : public Layer
        {
        private:
            const feature_t *activation_element; /*<! quantized alpha elements along channel axis >*/
            int activation_exponent;       /*<! exponent of quantized alpha elements >*/
            Tensor<feature_t> *output;     /*<! output ptr of prelu >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                false: the output will store to a separate memory >*/
            std::vector<int> output_shape; /*<! output shape of prelu >*/
        public:
            /**
             * @brief Construct a new PRelu object
             * 
             * @param activation_element   quantized alpha elements along channel axis
             * @param activation_exponent  exponent of quantized alpha elements
             * @param name                 name of prelu
             * @param inplace              true: the output will store to input0
             *                             false: the output will store to a separate memory
             */
            PRelu(const feature_t *activation_element,
                  const int activation_exponent = 0,
                  const char *name = "PRelu",
                  bool inplace = false) : Layer(name),
                                            activation_element(activation_element),
                                            activation_exponent(activation_exponent),
                                            output(NULL),
                                            inplace(inplace),
                                            output_shape({})
            {
            }

            /**
             * @brief Destroy the PRelu object
             * 
             */
            ~PRelu()
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
             * @return Tensor<feature_t>& PRelu result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call PRelu operation.
             * 
             * @param input       as an input
             * @param assign_core not effective yet
             * @return PRelu result
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
                    this->output->set_exponent(input.exponent);
                    this->output->malloc_element();
                    DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                    DL_LOG_LAYER_LATENCY_START();
                    nn::prelu(*this->output, input, this->activation_element, this->activation_exponent, assign_core);
                    DL_LOG_LAYER_LATENCY_END(this->name, "prelu");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    if (this->output->shape != this->output_shape)
                    {
                        this->output->set_shape(this->output_shape);
                    }
                    nn::prelu(*this->output, input, this->activation_element, this->activation_exponent, assign_core);
                    DL_LOG_LAYER_LATENCY_END(this->name, "prelu");
                }

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
