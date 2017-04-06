//
//  MGPredictor.cpp
//  MGPredictor
//
//  Created by soapyang on 2017/2/17.
//  Copyright © 2016年 MG. All rights reserved.
//
#include "MGPredictor.h"
#include "MGCMT.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>


using namespace cv;


namespace cmt
{
    Predictor::Predictor():KF(4,2,0),measurement(2,1)
    {
        correctTime = 0;
       
    }
    

    

    Predictor::~Predictor()
    {

        
    }
    

    void Predictor::initialize(Point2f pt)
    {
        KF.transitionMatrix = (Mat_<float>(4, 4) << 1,0,1,0,   0,1,0,1,  0,0,1,0,  0,0,0,1);
        
        measurement.setTo(Scalar(0));
        
        KF.statePre.at<float>(0) = pt.x;
        KF.statePre.at<float>(1) = pt.y;
        KF.statePre.at<float>(2) = 0;
        KF.statePre.at<float>(3) = 0;
        setIdentity(KF.measurementMatrix);
//        setIdentity(KF.processNoiseCov, Scalar::all(1e-4));
//        setIdentity(KF.measurementNoiseCov, Scalar::all(10));
//        setIdentity(KF.errorCovPost, Scalar::all(.1));
        

    }
    
    Point2f Predictor::correct(Point2f pt)
    {
        ++correctTime;
        preOrigCenter = curOrigCenter;
        curOrigCenter = pt;

        clock_t startCTime_compute = clock();
        measurement(0) = pt.x;
        measurement(1) = pt.y;
        
        // The update phase
        Mat estimated = KF.correct(measurement);
        Point2f statePt(estimated.at<float>(0),estimated.at<float>(1));
        
        if (TIMEDEBUG) {
            printf("CMTTIME  Predictor::correct:%.3f\n",(clock()-startCTime_compute)*1000.0/CLOCKS_PER_SEC);
        }
        
        err = pt - prePredictorCenter;

        return statePt;
    }
    
    Point2f Predictor::predict()
    {
        //return preOrigCenter;
        //
        Mat prediction = KF.predict();
        Point2f predictPt(prediction.at<float>(0),prediction.at<float>(1));
        
        if (TIMEDEBUG) {
            printf("EVENT  Predictor::predict:origPT(%.1f,%.1f) predictPT(%.1f,%.1f) 偏差(%.1f,%.1f)\n",preOrigCenter.x,preOrigCenter.y,predictPt.x,predictPt.y,preOrigCenter.x-predictPt.x,preOrigCenter.y - predictPt.y);
        }
        
        prePredictorCenter = predictPt;
        
        return predictPt;
    }
    
    Rect Predictor::calcPredictRect(cv::Size preRectSize,cv::Size img_size)
    {
        clock_t startCTime_compute = clock();
        
        Size2f preRectSizef(preRectSize.width,preRectSize.height);
        
        float scale = 2;
        Size2f s =  Size2f(preRectSize.width + fabs(err.x),preRectSize.height + fabs(err.y))*scale;
        Point2f pt = predict();
        

        Rect r(Point2f(pt.x - s.width/2,pt.y - s.height/2),s);
        
        if (TIMEDEBUG) {
            printf("CMTTIME  Predictor::calcPredictRect:%.3f\n",(clock()-startCTime_compute)*1000.0/CLOCKS_PER_SEC);
        }
        
        Point2f preTL = r.tl();
        Point2f preBR = r.br();
        
        if (preTL.x > img_size.width) {
            return Rect();
        }
        
        if (preTL.y > img_size.height) {
            return Rect();
        }
        
        if (preTL.x < 1) {
            preTL.x = 1;
        }
        
        if (preTL.y < 1) {
            preTL.y = 1;
        }
        
        if (preBR.x < 1 ) {
            return Rect();
        }
        
        if (preBR.y < 1 ) {
            return Rect();
        }
        
        if (preBR.x >  img_size.width - 2 ) {
            preBR.x = img_size.width - 2;
        }
        
        if (preBR.y > img_size.height - 2 ) {
            preBR.y = img_size.height - 2;
        }
        
        Rect goodR(preTL,preBR);
        
        return goodR;
    }
    
    void Predictor::keypointsFilter(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,Rect rect)
    {
         clock_t startCTime_compute = clock();
        
        for (size_t i = 0; i < srcKeypoints.size(); i++)
        {
            KeyPoint k = srcKeypoints[i];
            Point2f pt = k.pt;
            
            if (pt.x > rect.x && pt.y > rect.y && pt.x < rect.br().x && pt.y < rect.br().y)
            {
                dstKeypoints.push_back(k);
            }
        }
        
        if (TIMEDEBUG) {
            printf("CMTTIME  keypointsFilter::%.3f srcKeypoints:%ld dstKeypoints:%ld\n",(clock()-startCTime_compute)*1000.0/CLOCKS_PER_SEC,srcKeypoints.size(),dstKeypoints.size());
        }
    }


} /* namespace pa */
