#include "common_cmt.h"

using std::nth_element;

namespace cmt {
    
    //TODO: Check for even/uneven number of elements
    //The order of the elements of A is changed
    float median(vector<float> & A)
    {
        
        if (A.size() == 0)
        {
            return numeric_limits<float>::quiet_NaN();
        }
        
        nth_element(A.begin(), A.begin() + A.size()/2, A.end());
        
        return A[A.size()/2];
    }
    
    Point2f rotate(const Point2f v, const float angle)
    {
        Point2f r;
        r.x = cos(angle) * v.x - sin(angle) * v.y;
        r.y = sin(angle) * v.x + cos(angle) * v.y;
        
        return r;
    }
    
    void CalcClassWithTargetRect(const vector<cv::KeyPoint> &keypoints,const cv::Rect rect,int &fCount,int &bCount)
    {
        for (size_t i = 0; i < keypoints.size(); i++)
        {
            if (rect.contains(keypoints[i].pt))
            {
                fCount++;
            }
            else
            {
                bCount++;
            }
            
        }
    }
    
    void inoutRect(const std::vector<cv::KeyPoint>& keypoints, cv::Rect rect, std::vector<cv::KeyPoint>* pInArray, std::vector<cv::KeyPoint>* pOutArray)
    {
        for(unsigned int i = 0; i < keypoints.size(); i++)
        {
            if(rect.contains(keypoints[i].pt))
            {
                if (pInArray)
                {
                    pInArray->push_back(keypoints[i]);
                }
                
            }
            else
            {
                if (pOutArray)
                {
                    pOutArray->push_back(keypoints[i]);
                }
            }
        }
    }

    cv::Rect scaleRect(cv::Rect rect,float scale)
        {
            return cv::Rect(rect.x*scale,rect.y*scale,rect.width*scale,rect.height*scale);
        }

        long AlgorithmComplexity(int imgW,int imgH, int f1Count,int f2Count,int targetCount)
            {
                //光流+全局匹配+局部匹配
                long calcKPCount =  ((imgW*imgH)/10.0) + (targetCount*targetCount)*2 + f1Count*f1Count + targetCount*targetCount*2;

                return calcKPCount;
            }
    
    
} /* namespace cmt */




//
bool  TIMEDEBUG = true;
bool  CMT_perf_matcher_matchlocal = true;
