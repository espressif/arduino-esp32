#pragma once

#include "dl_image.hpp"

typedef struct
{
    int area;                /*!< Area of connected domains >*/
    std::vector<int> center; /*<! centroid of connected domains [x, y] >*/
    std::vector<int> box;    /*<! [left_up_x, left_up_y, right_down_x, right_down_y] >*/
} components_stats_t;

class ColorDetector
{
private:
    std::vector<std::vector<components_stats_t>> results; /*!< detection results >*/

public:
    std::vector<std::vector<uint8_t>> color_thresh; /*!< threshold of colors, The threshold of each color is composed of 6 numbers >*/
    std::vector<int> area_thresh;                   /*!< the area threshold of each color, 
                                                                the area that is smaller than the threshold is filtered >*/
    bool bgr;                                       /*!< true: the input image is in BGR format
                                                                false: the input image is in RGB format >*/

    /**
     * @brief get the color threshold of rectangular region in the image
     * 
     * @param image the input image
     * @param box   the coordinates of the rectanglar region : [left_up_x, left_up_y, right_down_x, right_down_y]
     * @return std::vector<uint8_t> the threshold.
     */
    std::vector<uint8_t> cal_color_thresh(dl::Tensor<uint8_t> &image, std::vector<int> box);

    /**
     * @brief detect the colors based on the color thresholds
     * 
     * @param image the input image.
     * @return std::vector<std::vector<components_stats_t>>&  detection result.
     */
    std::vector<std::vector<components_stats_t>> &detect(dl::Tensor<uint8_t> &image);

    /**
     * @brief Construct a new Color Detector object
     * 
     * @param color_thresh  threshold of colors, The threshold of each color is composed of 6 numbers
     * @param area_thresh   the area threshold of each color,the area that is smaller than the threshold is filtered
     * @param bgr           true: the input image is in BGR format
     *                      false: the input image is in RGB format
     */
    ColorDetector(std::vector<std::vector<uint8_t>> color_thresh, std::vector<int> area_thresh, bool bgr = false) : color_thresh(color_thresh), area_thresh(area_thresh), bgr(bgr)
    {
    }

    /**
     * @brief Destroy the Color Detector object
     * 
     */
    ~ColorDetector() {}

    /**
     * @brief Get the results object
     * 
     * @return std::vector<std::vector<components_stats_t>>& the detection result.
     */
    std::vector<std::vector<components_stats_t>> &get_results()
    {
        return this->results;
    }
};