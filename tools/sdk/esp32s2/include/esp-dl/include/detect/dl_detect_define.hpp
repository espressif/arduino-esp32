#pragma once

#include <vector>

namespace dl
{
    namespace detect
    {
        typedef struct
        {
            int category;              /*<! category index */
            float score;               /*<! score of box */
            std::vector<int> box;      /*<! [left_up_x, left_up_y, right_down_x, right_down_y] */
            std::vector<int> keypoint; /*<! [x1, y1, x2, y2, ...] */
        } result_t;
    }
}