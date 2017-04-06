#include "Fusion.h"

namespace cmt {

void Fusion::preferFirst(const vector<Point2f> & points_first, const vector<int> & classes_first,
    const vector<Point2f> & points_second, const vector<int> & classes_second,
    vector<Point2f> & points_fused, vector<int> & classes_fused)
{
    //FILE_LOG(logDEBUG) << "Fusion::preferFirst() call";
    points_fused = points_first;
    classes_fused = classes_first;
    if (false)
    {
        if (points_first.size() <=0 ) {
            return;
        }

        int max_class_first = INT_MIN;
        for (size_t j = 0; j < points_first.size(); j++)
        {
            int class_first = classes_first[j];
            if (max_class_first < class_first) {
                max_class_first = class_first;
            }
        }
        
        bool *class_first_flag = new bool[max_class_first + 1];
        for (int i=0; i<max_class_first + 1; ++i) {
            class_first_flag[i] = false;
        }
        for (size_t j = 0; j < points_first.size(); j++)
        {
            int class_first = classes_first[j];
            class_first_flag[class_first] = true;
        }
        
        for (size_t i = 0; i < points_second.size(); i++) {
            int class_second = classes_second[i];
            bool found = class_first_flag[class_second];
            
            if (!found)
            {
                points_fused.push_back(points_second[i]);
                classes_fused.push_back(class_second);
            }
        }
        printf("points_fused.size %lu\n",points_fused.size());
        delete []class_first_flag;
    } else {
        for (size_t i = 0; i < points_second.size(); i++)
        {
            int class_second = classes_second[i];
            
            bool found = false;
            for (size_t j = 0; j < points_first.size(); j++)
            {
                int class_first = classes_first[j];
                if (class_first == class_second) found = true;
            }
            
            if (!found)
            {
                points_fused.push_back(points_second[i]);
                classes_fused.push_back(class_second);
            }
            
        }
        printf("points_fused.size %lu\n",points_fused.size());
    }
    //FILE_LOG(logDEBUG) << "Fusion::preferFirst() return";
}

} /* namespace cmt */
