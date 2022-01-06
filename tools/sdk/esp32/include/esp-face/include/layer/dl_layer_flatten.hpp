#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_tool.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief 
         * 
         * @tparam feature_t 
         */
        template <typename feature_t>
        class Flatten : public Layer
        {
        private:
            int output_exponent;           /*<! exponent of output >*/
            Tensor<feature_t> *output;     /*<! output ptr of Flatten >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                false: the output will store to a separate memory >*/
            std::vector<int> output_shape; /*<! output shape of Flatten >*/

        public:
            /**
             * @brief Construct a new Flatten object
             * 
             * @param name name of layer
             * @param inplace true: the output will store to input0
             *                false: the output will store to a separate memory
             */
            Flatten(const char *name = "Flatten", bool inplace = false) : Layer(name), output(NULL), inplace(inplace), output_shape({})
            {}

            /**
             * @brief Destroy the Flatten object
             * 
             */
            ~Flatten()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
            * @brief  Update output shape.
            * 
            * @param input as an input
            * @param print_shape  whether to print the output shape. 
            */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                this->output_exponent = input.exponent;
                this->output_shape = {input.get_size()};
                if (!this->inplace)
                {
                    if (this->output == NULL)
                    {
                        this->output = new Tensor<feature_t>;
                    }
                    this->output->set_exponent(this->output_exponent);
                    this->output->set_shape(this->output_shape);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input;
                    this->output->set_shape(this->output_shape);
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
             * @return Tensor<feature_t>& Flatten result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Flatten operation.
             * 
             * @param input as an input
             * @return Tensor<feature_t>& Flatten result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if (!this->inplace)
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->set_exponent(input.exponent);
                    this->output->flatten();
                    this->output->copy_element(input, true);
                    DL_LOG_LAYER_LATENCY_END(this->name, "flatten");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->flatten();
                    DL_LOG_LAYER_LATENCY_END(this->name, "flatten");
                }
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
