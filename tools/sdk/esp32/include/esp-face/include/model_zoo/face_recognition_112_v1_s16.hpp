#pragma once

#include "dl_variable.hpp"
#include "face_recognition_tool.hpp"
#include "face_recognizer.hpp"
#include <vector>

using namespace dl;

/**
 * @brief face recognition model v1
 * input size: 112 x 112 x 3
 * quantization mode: S16
 * 
 */
class FaceRecognition112V1S16 : public FaceRecognizer<int16_t>
{       
    public:
        /**
         * @brief Construct a new Face_Recognition_112_V1_S16 object
         * 
         */
        FaceRecognition112V1S16();
        
        /**
         * @brief Destroy the Face_Recognition_112_V1_S16 object
         * 
         */
        ~FaceRecognition112V1S16();
};