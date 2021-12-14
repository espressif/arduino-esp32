#pragma once

#include "dl_image.hpp"

typedef struct
{
    int area;                /*!< Area of connected domains >*/
    std::vector<int> center; /*<! centroid of connected domains [x, y] >*/
    std::vector<int> box;    /*<! [left_up_x, left_up_y, right_down_x, right_down_y] >*/
} color_detect_result_t;

typedef struct
{
    std::vector<int> start_col;
    std::vector<int> end_col;
    std::vector<int> row;
    std::vector<int> index;
    std::vector<int> area;
} color_segment_result_t;

typedef struct
{
    std::vector<uint8_t> color_thresh; /*!< threshold of colors, The threshold of each color is composed of 6 numbers >*/
    int area_thresh;                   /*!< the area threshold of each color, 
                                            the area that is smaller than the threshold is filtered >*/
    std::string name;                  /*!<name of the color>*/
} color_info_t;

class ColorDetector
{
private:
    std::vector<std::vector<color_detect_result_t>> detection_results; /*!< detection results >*/
    std::vector<color_segment_result_t> segmentation_results;          /*!< segmentation results >*/
    std::vector<color_info_t> registered_colors;                       /*!< the infomation of registered colors >*/
    std::vector<uint8_t> color_thresh_offset;                          /*!< HSV offset of the registered colors>*/
    std::vector<int> detection_shape;                                  /*!< the inference shape of images, the input image will be resized to this shape. 
                                                                    if the shape == {}, the input image will not be resized >*/
    bool bgr;                                                          /*!< true: the input image is in BGR format
                                                                false: the input image is in RGB format >*/
    int id_nums;                                                       /*!< the number of registered colors in history>*/
    float h_ratio;
    float w_ratio;
    void color_detection_forward(dl::Tensor<uint8_t> &bin, int area_thresh);

public:
    /**
     * @brief get the color threshold of rectangular region in the image
     * 
     * @param image the input image in RGB888 format.
     * @param box   the coordinates of the rectanglar region : [left_up_x, left_up_y, right_down_x, right_down_y]
     * @return std::vector<uint8_t> the threshold.
     */
    std::vector<uint8_t> cal_color_thresh(dl::Tensor<uint8_t> &image, std::vector<int> box);

    /**
     * @brief get the color threshold of rectangular region in the image
     * 
     * @param input the ptr of RGB565 image.
     * @param input_shape shape of the input image.
     * @param box the coordinates of the rectanglar region : [left_up_x, left_up_y, right_down_x, right_down_y]
     * @return std::vector<uint8_t> the threshold.
     */
    std::vector<uint8_t> cal_color_thresh(uint16_t *input, std::vector<int> input_shape, std::vector<int> box);

    /**
     * @brief register a new color to the color detector
     * 
     * @param image the input image in RGB888 format.
     * @param box the coordinates of the rectanglar region : [left_up_x, left_up_y, right_down_x, right_down_y]
     * @param area_thresh the area threshold of the color
     * @param id the index of the color
     * @return int the number of the registered colors. if the id is not valid, return -1.
     */
    int register_color(dl::Tensor<uint8_t> &image, std::vector<int> box, int area_thresh = 256, std::string color_name = "", int id = -1);

    /**
     * @brief register a new color to the color detector
     * 
     * @param input the ptr of RGB565 image.
     * @param input_shape shape of the input image.
     * @param box the coordinates of the rectanglar region : [left_up_x, left_up_y, right_down_x, right_down_y]
     * @param area_thresh the area threshold of the color
     * @param id the index of the color
     * @return int the number of the registered colors. if the id is not valid, return -1.
     */
    int register_color(uint16_t *input, std::vector<int> input_shape, std::vector<int> box, int area_thresh = 256, std::string color_name = "", int id = -1);

    /**
     * @brief register a new color to the color detector
     * 
     * @param color_thresh the color threshold
     * @param area_thresh the area threshold of the color
     * @param id the index of the color
     * @return int the number of the registered colors. if the id is not valid, return -1.
     */
    int register_color(std::vector<uint8_t> color_thresh, int area_thresh = 256, std::string color_name = "", int id = -1);

    /**
     * @brief delete a registered color
     * 
     * @param id the index of the color
     * @return int the number of the registered colors. if the id is not valid, return -1.
     */
    int delete_color(int id = -1);

    /**
     * @brief delete a registered color
     * 
     * @param color_name  name of the registered_color
     * @return int the number of the registered colors. if the id is not valid, return -1.
     */
    int delete_color(std::string color_name);

    /**
     * @brief delete all the registered colors
     * 
     */
    void clear_color();

    /**
     * @brief detect the colors based on the color thresholds
     * 
     * @param image the input image.
     * @return std::vector<std::vector<color_detect_result_t>>&  detection result.
     */
    std::vector<std::vector<color_detect_result_t>> &detect(dl::Tensor<uint8_t> &image, std::vector<int> color_ids = {});

    /**
     * @brief 
     * 
     * @param input 
     * @param input_shape 
     * @return std::vector<std::vector<color_detect_result_t>>& 
     */
    std::vector<std::vector<color_detect_result_t>> &detect(uint16_t *input_shape, std::vector<int> shape, std::vector<int> color_ids = {});

    /**
     * @brief Construct a new Color Detector object
     * 
     * @param color_thresh_offset   HSV offset of the registered colors>
     * @param detection_shape   the inference shape of images, the input image will be resized to this shape
     * @param bgr           true: the input image is in BGR format
     *                      false: the input image is in RGB format
     */
    ColorDetector(std::vector<uint8_t> color_thresh_offset = {}, std::vector<int> detection_shape = {}, bool bgr = true) : color_thresh_offset(color_thresh_offset),
                                                                                                                           detection_shape(detection_shape), bgr(bgr), id_nums(0)
    {
    }

    /**
     * @brief Destroy the Color Detector object
     * 
     */
    ~ColorDetector() {}

    /**
     * @brief Get the detection results object
     * 
     * @return std::vector<std::vector<color_detect_result_t>>& the detection result.
     */
    std::vector<std::vector<color_detect_result_t>> &get_detection_results()
    {
        return this->detection_results;
    }

    /**
     * @brief Get the segmentation results object
     * 
     * @return std::vector<color_segment_result_t>& the segmentation result.
     */
    std::vector<color_segment_result_t> &get_segmentation_results()
    {
        return this->segmentation_results;
    }

    /**
     * @brief Get the registered colors object
     * 
     * @return std::vector<color_info_t> the information of resgistered colors
     */
    std::vector<color_info_t> get_registered_colors()
    {
        return this->registered_colors;
    }

    /**
     * @brief Set the color thresh offset object
     * 
     * @param color_thresh_offset the offset of color thresh for registered colors
     * @return ColorDetector& 
     */
    ColorDetector &set_color_thresh_offset(std::vector<uint8_t> color_thresh_offset)
    {
        assert(color_thresh_offset.size() == 3);
        this->color_thresh_offset = color_thresh_offset;
        return *this;
    }

    /**
     * @brief Get the color thresh offset object
     * 
     * @return std::vector<uint8_t> color_thresh_offset
     */
    std::vector<uint8_t> get_color_thresh_offset()
    {
        return this->color_thresh_offset;
    }

    /**
     * @brief Set the area thresh object
     * 
     * @param area_thresh the area thresh for each registered colors
     * @return ColorDetector& 
     */
    ColorDetector &set_area_thresh(std::vector<int> area_thresh)
    {
        assert((area_thresh.size() == this->registered_colors.size()) || (area_thresh.size() == 1));
        if (area_thresh.size() == 1)
        {
            for (int i = 0; i < this->registered_colors.size(); ++i)
            {
                this->registered_colors[i].area_thresh = area_thresh[0];
            }
        }
        else
        {
            for (int i = 0; i < this->registered_colors.size(); ++i)
            {
                this->registered_colors[i].area_thresh = area_thresh[i];
            }
        }
        return *this;
    }

    /**
     * @brief Set the area thresh object
     * 
     * @param area_thresh the area thresh for each registered colors
     * @param id          index of the registered color
     * @return ColorDetector& 
     */
    ColorDetector &set_area_thresh(int area_thresh, int id)
    {
        assert((id >= 0) && (id < this->registered_colors.size()));
        this->registered_colors[id].area_thresh = area_thresh;
        return *this;
    }

    /**
     * @brief Set the bgr object
     * 
     * @param bgr 
     * @return ColorDetector& 
     */
    ColorDetector &set_bgr(bool bgr)
    {
        this->bgr = bgr;
        return *this;
    }

    /**
     * @brief Get the bgr object
     * 
     * @return bool bgr flag
     */
    bool get_bgr()
    {
        return this->bgr;
    }

    /**
     * @brief Get the detection shape object
     * 
     * @return std::vector<int> 
     */
    std::vector<int> get_detection_shape()
    {
        return this->detection_shape;
    }

    /**
     * @brief Set the detection shape object
     * 
     * @param detection_shape the inference shape of images, the input image will be resized to this shape
     * @return ColorDetector& 
     */
    ColorDetector &set_detection_shape(std::vector<int> detection_shape)
    {
        assert(detection_shape.size() == 3);
        this->detection_shape = detection_shape;
        return *this;
    }

    /**
     * @brief Get the registered colors num
     * 
     * @return int the registered colors num
     */
    int get_registered_colors_num()
    {
        return this->registered_colors.size();
    }

    /**
     * @brief print the detection detection results
     * 
     * @param tag
     */
    void print_detection_results(const char *tag = "RGB")
    {
        printf("\n%s | color detection result:\n", tag);
        for (int i = 0; i < this->detection_results.size(); ++i)
        {
            printf("color %d: detected box :%d\n", i, this->detection_results[i].size());
            for (int j = 0; j < this->detection_results[i].size(); ++j)
            {
                printf("center: (%d, %d)\n", this->detection_results[i][j].center[0], this->detection_results[i][j].center[1]);
                printf("box: (%d, %d), (%d, %d)\n", this->detection_results[i][j].box[0], this->detection_results[i][j].box[1], this->detection_results[i][j].box[2], this->detection_results[i][j].box[3]);
                printf("area: %d\n", this->detection_results[i][j].area);
            }
            printf("\n");
        }
    }

    /**
     * @brief print the segmentation results
     * 
     * @param tag 
     */
    void print_segmentation_results(const char *tag = "RGB")
    {
        printf("\n%s | color segmentation result:\n", tag);
        for (int i = 0; i < this->segmentation_results.size(); ++i)
        {
            printf("color %d: detected box :%d\n", i, this->detection_results[i].size());
            for (int j = 0; j < this->segmentation_results[i].index.size(); ++j)
            {
                printf("box_index: %d, start col: %d, end col: %d, row: %d, area: %d\n",
                       this->segmentation_results[i].index[j], this->segmentation_results[i].start_col[j], this->segmentation_results[i].end_col[j],
                       this->segmentation_results[i].row[j], this->segmentation_results[i].area[j]);
            }
            printf("\n");
        }
    }

    /**
     * @brief draw the color segmentation result on the input image
     * 
     * @param image                  the input RGB image
     * @param draw_colors            RGB values for each detected colors
     * @param draw_backgound         draw the background if it is true
     * @param background_color       RGB values for the background color
     */
    void draw_segmentation_results(dl::Tensor<uint8_t> &image, std::vector<std::vector<uint8_t>> draw_colors, bool draw_backgound = true, std::vector<uint8_t> background_color = {0, 0, 0});

    /**
     * @brief draw the color segmentation result on the input image
     * 
     * @param image                 the pointer of the input RGB565 image
     * @param image_shape           the shape of the input image
     * @param draw_colors           RGB565 values for  each detected colors
     * @param draw_backgound        draw the background if it is true
     * @param background_color      RGB565 values for the background color
     */
    void draw_segmentation_results(uint16_t *image, std::vector<int> image_shape, std::vector<uint16_t> draw_colors, bool draw_backgound = true, uint16_t background_color = 0x0000);
};