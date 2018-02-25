//
//  MGLearnT.h
//  MGLearnT
//
//  Created by soapyang on 2017/2/17.
//  Copyright © 2016年 MG. All rights reserved.
//
#ifndef MGLearnT_H
#define MGLearnT_H

#include "common_cmt.h"
#include "Consensus.h"
#include "Fusion.h"
#include "Matcher.h"
#include "Tracker.h"
#include "MGPredictor.h"

//性能平衡系统
#include <opencv2/features2d/features2d.hpp>
#include "MGPerformanceAdapter.h"


//因为类成员变量的命名和局部变量相同，发生几次难查的bug
//从17.2.24起，类成员变量用开头用下划线


using cv::FeatureDetector;
using cv::DescriptorExtractor;
using cv::RotatedRect;
using cv::Size2f;

namespace cmt
{
    
    class MGLearnT
    {
    public:
        MGLearnT();
        ~MGLearnT();
    public:
        bool initialize(const Mat grayImage,vector<KeyPoint> &keypoints, const cv::Rect rect,const PerformanceAdapter &perfAdapter);
        bool update(const Mat grayImage, vector<KeyPoint> &keypoints,const cv::Rect targetRect,bool filteKPT = true);
        
        bool setMatchedParam(float curScale);
        
        //预测匹配
        void processFrameWithKeypoints(bool all,cv::Mat &grayImage,vector<KeyPoint> &keypoints, Mat descriptors);
        void track(const Mat &im_prev, const Mat &im_gray, const vector<Point2f> & points_prev,
                   vector<Point2f> & points_tracked, vector<unsigned char> & status,vector<int> &classes_tracked);
        
        
        void clear();
        
    public:
        
        Fusion _fusion;   // 数据融合器
        Matcher _matcher; // 特征匹配器
        Tracker _tracker; // 跟踪器
        Consensus _consensus;  // 一致检查器
        PerformanceAdapter _perfAdapter; //性能适配器
        
        vector<Point2f> _activePoints; //public for visualization purposes 有效特征点
        RotatedRect _outRot;       //需要反算缩放输出
        RotatedRect _innerRot; //内部计算用，未经过缩放
        
    public:
        
        bool  _isReady;
        //判断跟踪结果
        bool  _isPreMatched;
        bool  _isMatched;
        float _matchedPrecent; //匹配百分比，衡量跟踪优劣
        
        float _curTargetScale;
        
        int _origTargetLength; //目标特征长度
        
    public:
        
        bool _isDebugModel;
        Mat  *_pDebugMatAllSize;
    
        
        
    protected:
    
        bool setup(const Mat grayImage, vector<KeyPoint> &keypoints,const cv::Rect targetRect);
        
    protected:
        cv::Ptr<FeatureDetector> detector;
        cv::Ptr<DescriptorExtractor> descriptor;
        
        cv::Rect _targetRect;  // 初始大小
        vector<int> _activeClasses;
        
        Mat _preImage;
       
    };
    
} /* namespace CMT */

#endif /* end of include guard: CMT_H */
