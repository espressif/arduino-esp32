#pragma once

#include <list>
#include <vector>
#include "dl_detect_define.hpp"

class HumanFaceDetectMSR01
{
private:
    void *model;

public:
    /**
     * @brief Construct a new Human Face Detect MSR01 object
     * 
     * @param score_threshold   predicted boxes with score lower than the threshold will be filtered out
     * @param nms_threshold     predicted boxes with IoU higher than the threshold will be filtered out
     * @param top_k             first k highest score boxes will be remained
     * @param resize_scale      resize scale to implement on input image
     */
    HumanFaceDetectMSR01(const float score_threshold, const float nms_threshold, const int top_k, float resize_scale);

    /**
     * @brief Destroy the Human Face Detect MSR01 object
     */
    ~HumanFaceDetectMSR01();

    /**
     * @brief Inference.
     * 
     * @tparam T supports uint8_t and uint16_t
     *         - uint8_t: input image is RGB888
     *         - uint16_t: input image is RGB565
     * @param input_element pointer of input image
     * @param input_shape   shape of input image
     * @return detection result
     */
    template <typename T>
    std::list<dl::detect::result_t> &infer(T *input_element, std::vector<int> input_shape);
};
