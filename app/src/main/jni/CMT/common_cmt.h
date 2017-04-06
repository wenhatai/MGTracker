#ifndef COMMON_H

#define COMMON_H

#include <limits>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>


#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>


using cv::Mat;
using cv::Point2f;
using cv::Size2f;
using cv::KeyPoint;

using std::numeric_limits;
using std::string;
using std::vector;
using std::list;
using std::set;
using std::map;
using std::pair;


namespace cmt
{
    float median(vector<float> & A);
    Point2f rotate(const Point2f v, const float angle);
    void inoutRect(const std::vector<cv::KeyPoint>& keypoints, cv::Rect rect, std::vector<cv::KeyPoint>* pInArray, std::vector<cv::KeyPoint>* pOutArray);
    
    
    
    
    template<class T>
    int sgn(T x)
    {
        if (x >=0) return 1;
        else return -1;
    }
    
    
    cv::Rect scaleRect(cv::Rect rect,float scale);

    
    long AlgorithmComplexity(int imgW,int imgH, int f1Count,int f2Count,int targetCount);

    
    

} /* namespace cmt */

extern "C" {
    extern double MGXPrefLogGetNowTime();
    extern void MGXPrefLog(const char *funKey,const char *desc,double timeVar,bool needSumParam);
    
    extern bool  CMT_perf_matcher_matchlocal;
    extern bool  TIMEDEBUG;
    
}




#define USER_FASTX 1

#define m_str_detector "FAST"
//#define m_str_detector "BRISK"
#define m_str_descriptor "BRISK"

#define MGT_DescriptorMatcherType "BruteForce-Hamming"

#endif /* end of include guard: COMMON_H */
