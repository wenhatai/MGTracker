//
//  MGPredictor.h
//  MGPredictor
//
//  Created by soapyang on 2017/2/17.
//  Copyright © 2016年 MG. All rights reserved.
//
#ifndef Predictor_H

#define Predictor_H

#include "common_cmt.h"
#include "Consensus.h"
#include "Fusion.h"
#include "Matcher.h"
#include "Tracker.h"

#include <opencv2/features2d/features2d.hpp>
#include <opencv2/video/tracking.hpp>




using cv::FeatureDetector;
using cv::DescriptorExtractor;

using cv::RotatedRect;
using cv::Size2f;
using cv::KalmanFilter;
using namespace cv;

namespace cmt
{
    
    //预测器
    class Predictor
    {
    public:
        Predictor()  ;
        ~Predictor();
        
        void initialize(Point2f pt);
        
    public:
        
        Point2f correct(Point2f pt);
        Point2f predict();
        
        cv::Rect calcPredictRect(cv::Size preRectSize,cv::Size img_size);
    
        void keypointsFilter(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,cv::Rect rect);
        
        bool canUse(){return correctTime > 3;};
   
        
    public:
        int correctTime;
        KalmanFilter KF;
        Mat_<float> measurement;
        
        
        Point2f curOrigCenter;
        Point2f preOrigCenter;
    
        Point2f prePredictorCenter;
        Point2f err;
        
        

        
    };
    
    
} /* namespace Predictor_H */

#endif /* end of include guard: Predictor_H */
