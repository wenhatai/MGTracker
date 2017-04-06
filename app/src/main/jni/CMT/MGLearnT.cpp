//
//  MGLearnT.cpp
//  MGLearnT
//
//  Created by soapyang on 2017/2/17.
//  Copyright © 2016年 MG. All rights reserved.
//
#include "MGLearnT.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>



namespace cmt
{
    
    static bool calcIsMatched(float curScale ,float prePreTargetScale,float matchedPrecentTemp)
    {
        bool isMatched = false;
        
        if (matchedPrecentTemp  > 0.3) //0.1
        {
            isMatched = true;
        }
        else if (matchedPrecentTemp  > 0.15)
        {
            float changed = curScale/prePreTargetScale;
            if (changed < 1.5 && changed > 0.3 && curScale < 5)
            {
                isMatched = true;
            }
        }
        
        return isMatched;
    }
    
    
    
    MGLearnT::MGLearnT()
    {
        printf("EVENT MGLearnT 创建 %p\n",this);
  
        _isMatched = false;
        _isPreMatched = false;
        
        _matchedPrecent = 0; //匹配百分比，衡量跟踪优劣
        _curTargetScale = 1;
        
        //debug
        _isDebugModel = true;
        _pDebugMatAllSize = NULL;
    
     
        //Initialize detector and descriptor 初始化检测器FAST和描述器BRISK
        #if CV_MAJOR_VERSION > 2
            descriptor = cv::BRISK::create();
        #else
            descriptor = new cv::BRISK;
        #endif
        
        _isReady = false;
        
        _origTargetLength = 0;
        
    }
    
    MGLearnT::~MGLearnT()
    {
        printf("EVENT MGLearnT 析构 %p\n",this);
    }
    
    bool MGLearnT::initialize(const Mat grayImage, vector<KeyPoint> &keypoints, const cv::Rect rect ,const PerformanceAdapter &perfAdapter)
    {

        _perfAdapter = perfAdapter;
        _origTargetLength = 0;

        //
        bool isTargetHasKeypointDesc = update(grayImage, keypoints, rect,false);
        
        _origTargetLength = _matcher.target_length;
    
        return isTargetHasKeypointDesc;
    }
    
    
    void MGLearnT::clear()
    {
        //清理所有中间状态
        _outRot = RotatedRect();
        _innerRot = RotatedRect();
        
        _activePoints.clear();
        _activeClasses.clear();
        
        _isMatched = false;
        _isPreMatched = false;
        _matchedPrecent = 0.0;
        
        //类要重建
        _fusion = Fusion();
        _matcher = Matcher();
        _tracker = Tracker();
        _consensus = Consensus();
        _perfAdapter = PerformanceAdapter();
        //初始化
        
        _isReady = false;
    }
    
    static int getMinTargetKPTCount(int origCount)
    {
        int r = origCount;
        
        if (origCount < 20)
        {
            r = origCount*0.9;
        }
        else if(origCount >= 20 && origCount < 50)
        {
            r = origCount*0.8;
        }
        else if(origCount >= 50 && origCount < 100)
        {
            r = origCount*0.7;
        }
        else if(origCount >= 100 && origCount < 200)
        {
            r = origCount*0.6;
        }
        else
        {
            r = origCount*0.5;
        }
        
        return r;
    }
    
    
    bool MGLearnT::update(const Mat grayImage, vector<KeyPoint> &keypointsTemp,const cv::Rect targetRect,bool filteKPT /*= true*/)
    {
        vector<KeyPoint> &keypoints = keypointsTemp;
        vector<KeyPoint> goodKeypoints;
        if(filteKPT)
        {
            _perfAdapter.calcTargetKeypointsResponseDistribution(keypointsTemp, targetRect);
            _perfAdapter.keypointsFilterByRegionalDensityThresholdDistribution(keypointsTemp, goodKeypoints, grayImage, _perfAdapter._initScale, _perfAdapter._initKPTRegionalDensity);
            keypoints = goodKeypoints;
        }
        
        //分离出跟踪框和背景的关键点
        std::vector<cv::KeyPoint> targetKeypoints;
        std::vector<cv::KeyPoint> backgroundKeypoints;
        
        inoutRect(keypoints, targetRect, &targetKeypoints, &backgroundKeypoints);
        
        if (targetKeypoints.size()  < getMinTargetKPTCount( _origTargetLength))
        {
            return false;
        }
        
        
        //存储跟踪区域的初始大小
        _targetRect = targetRect;
        
        //存储初始灰度图像
        _preImage = grayImage;
        
        //计算跟踪区域的中心位置
        Point2f center = Point2f(targetRect.x + targetRect.width/2.0, targetRect.y + targetRect.height/2.0);
        
        
        //计算跟踪框和背景的特征
        Mat targetDescs;
        Mat backgroundDescs;
        descriptor->compute(grayImage, targetKeypoints, targetDescs);
        
  
        
        descriptor->compute(grayImage, backgroundKeypoints, backgroundDescs);
        
        //创建跟踪框索引
        vector<int> targetClasses;
        targetClasses.reserve(targetKeypoints.size());
        for (size_t i = 0; i < targetKeypoints.size(); i++)
        {
            targetClasses.push_back((int)i);
        }
        
        //提取关键点中的点
        vector<Point2f> targetPoints;
        //vector<Point2f> backgroundPoints;
        
        for (size_t i = 0; i < targetKeypoints.size(); i++)
        {
            targetPoints.push_back(targetKeypoints[i].pt);
        }
        
        
//        for (size_t i = 0; i < backgroundKeypoints.size(); i++)
//        {
//            backgroundPoints.push_back(backgroundKeypoints[i].pt);
//        }
        
        //创建正规化的点，即计算前景的关键点到前景框中的相对位置作为正规化的点的坐标
        vector<Point2f> normalizedPoints;
        for (size_t i = 0; i < targetPoints.size(); i++)
        {
            normalizedPoints.push_back(targetPoints[i] - center);
        }
        
        //Initialize matcher 初始化匹配器
        _matcher.initialize(normalizedPoints, targetDescs, targetClasses, backgroundDescs, center);
        
        //learnMatcher.initialize(keypoints_fg, descs_fg, points_normalized, size_initial, center);
        
        //Initialize consensus 初始化一致器
        _consensus = Consensus();
        //_consensus.estimate_scale = false;
        _consensus.estimate_rotation = false;
        _consensus.initialize(normalizedPoints);
        
        
        //Create initial set of active keypoints 创建初始的活动点，即前景关键点的坐标
        _activePoints = targetPoints;
        _activeClasses = targetClasses;

        _isReady = true;
        
        return true;
    }

        
    
    void MGLearnT::track(const Mat& im_prev1, const Mat& im_gray2, const vector<Point2f> & points_prev,
                    vector<Point2f> & points_tracked, vector<unsigned char> & status,vector<int> &classes_tracked)
    {
        //return;
        //MGX_PERF_LOG_BEGIN_TIME(trackertrack)
        
        clock_t startCTime_track  = clock();
        //Track keypoints
        
        // 利用光流法计算关键点的当前位置。
        
        _tracker.track(im_prev1, im_gray2, points_prev, points_tracked, status);
        
        
        for (size_t i = 0; i < _activeClasses.size(); i++)
        {
            if (status[i])
            {
                classes_tracked.push_back(_activeClasses[i]);
            }
            
        }

        if (TIMEDEBUG)
        {
            
            char log[40] = {0};
            sprintf(log, "光流tCount:%ld imgsize(x:%d,y:%d)",classes_tracked.size(),im_prev1.cols,im_gray2.rows);
            
            //MGX_PERF_LOG_APP("track", log, trackertrack, false);
            
            printf("CMTTIME track:%.3f tCount:%ld imgsize(x:%d,y:%d) \n",(clock()-startCTime_track)*1000.0/CLOCKS_PER_SEC,classes_tracked.size(),im_prev1.cols,im_gray2.rows);
        }
        
    }
    
    
    void MGLearnT::processFrameWithKeypoints(bool all,cv::Mat &grayImage,vector<KeyPoint> &keypoints, Mat descriptors)
    {
        cv::Mat preGrayImage = _preImage;
        _preImage = grayImage;
        if (keypoints.size() <= 0)
        {
            return;
        }
        
        //光流
        vector<Point2f> points_tracked;
        vector<unsigned char> status;
        vector<int> classes_tracked;
        
        track(preGrayImage, grayImage, _activePoints, points_tracked, status,classes_tracked);
        
        //=======
        
        clock_t startCTime_all = clock();
        clock_t startCTime = clock();
        //MGX_PERF_LOG_BEGIN_TIME(matchGlobal)
        
        //在全局和之前的数据库匹配特征点，计算出匹配的特征点
        vector<Point2f> points_matched_global;
        vector<int> classes_matched_global;
        vector<int>  curkpts_classes_matched;
        _matcher.matchGlobal(keypoints, descriptors, points_matched_global, classes_matched_global,curkpts_classes_matched);
        

        if (TIMEDEBUG) {
            //MGX_PERF_LOG_APP("MGLearn TmatchGlobal", "匹配全局特征点", matchGlobal, false);
            printf("CMTTIME MGLearn matchGlobal:%.3f\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = clock();

        
        //Fuse tracked and globally matched points
        //融合跟踪和匹配的点
        //MGX_PERF_LOG_BEGIN_TIME(preferFirst)
        vector<Point2f> points_fused;
        vector<int> classes_fused;
        
        _fusion.preferFirst(points_tracked, classes_tracked, points_matched_global, classes_matched_global,
                           points_fused, classes_fused);
        

        if (TIMEDEBUG) {
            //MGX_PERF_LOG_APP("MGLearn preferFirst", "融合", preferFirst, false);
            printf("CMTTIME MGLearn preferFirst:%.3f\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = clock();
        //MGX_PERF_LOG_BEGIN_TIME(estimateScaleRotation)
        
        // 估计旋转和缩放利用最终的融合点
        float scale = 1.0f;
        float rotation = 0;
        
        _consensus.estimateScaleRotation(points_fused, classes_fused, scale, rotation);
       
        if (TIMEDEBUG)
        {
            //MGX_PERF_LOG_APP("estimateScaleRotation", "估计旋转和缩放", estimateScaleRotation, false);
            printf("CMTTIME estimateScaleRotation:%.3f\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        
        startCTime = clock();
        //MGX_PERF_LOG_BEGIN_TIME(findConsensus)
        //FILE_LOG(logDEBUG) << "scale " << scale << ", " << "rotation " << rotation;
        
        //Find inliers and the center of their votes
        // votes : pointsFused - DeformsFG
        // votes = pointsFused - scale * rotate(vectorsFG[classes[it]], rotation);
        
        //计算一致性，获取相关的点inliers和中心
        Point2f center;
        vector<Point2f> points_inlier;
        vector<int> classes_inlier;
        _consensus.findConsensus(points_fused, classes_fused, scale, rotation,
                                center, points_inlier, classes_inlier);
        
        
        if (TIMEDEBUG)
        {
            //MGX_PERF_LOG_APP("preferFirst", "计算一致性", estimateScaleRotation, false);
            printf("CMTTIME findConsensus:%.3f\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        
        //MGX_PERF_LOG_BEGIN_TIME(matchLocal)
        startCTime = clock();
 
        //Match keypoints locally 局部匹配
        vector<Point2f> points_matched_local;
        vector<int> classes_matched_local;
        vector<int>  curkpts_classes_matched_local;
        _matcher.matchLocal(keypoints, descriptors, center, scale, rotation, points_matched_local, classes_matched_local,curkpts_classes_matched_local);
        
 
        if (TIMEDEBUG) {
            //MGX_PERF_LOG_APP("matchLocal", "局部匹配", matchLocal, false);
            printf("CMTTIME matchLocal:%.3f \n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        
        //MGX_PERF_LOG_BEGIN_TIME(preferFirst2)
        startCTime = clock();

        
        // 融合局部匹配的点和inliers
        _fusion.preferFirst(points_matched_local, classes_matched_local, points_inlier, classes_inlier, _activePoints, _activeClasses);

        if (TIMEDEBUG)
        {
            printf("CMTTIME preferFirst:%.3f points_inlier:%lu points_matched_local:%lu points_active:%lu\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC,points_inlier.size(),points_matched_local.size(),_activePoints.size());
        }
        
        //MGX_PERF_LOG_APP("preferFirst2", "融合局部匹配的点和inliers", preferFirst2, false);
        

        //计算匹配程度
        setMatchedParam(scale);
        
        if (!_isMatched)
        {
            return ;
        }

        
        startCTime = clock();

        // 计算出新的跟踪窗口
        cv::Size2f rectSize(_targetRect.size().width,_targetRect.size().height);
        _outRot = RotatedRect(center ,  rectSize* scale , rotation/CV_PI * 180);
        _innerRot = _outRot;
        
        
        
        if (TIMEDEBUG)
        {
            printf("CMTTIME MGLearnT %s 总耗时:%.3f \n",all?"全部检测成功":"预测检测成功",(clock()-startCTime_all)*1000.0/CLOCKS_PER_SEC);
        }
        //FILE_LOG(logDEBUG) << "CMT::processFrame() return";
        
        if (_isDebugModel)
        {
            //最后活动的特征点
            for(size_t i = 0; i < _activePoints.size(); i++)
            {
                circle(*_pDebugMatAllSize, _activePoints[i]*(1.0/_perfAdapter._initScale) , 2, cv::Scalar(255,0,255,255));
            }
            
            RotatedRect rect = _innerRot;
            Point2f vertices[4];
            rect.points(vertices);
            for (int i = 0; i < 4; i++)
            {
               
                
                line(*_pDebugMatAllSize, vertices[i]*(1.0/_perfAdapter._initScale), vertices[(i+1)%4]*(1.0/_perfAdapter._initScale), cv::Scalar(255,0,255,255));
            }
            
            //rectangle(debugMat, rect.boundingRect() ,cv::Scalar(0,255,0,255));
        }
    
    }
    

    bool MGLearnT::setMatchedParam(float curScale)
    {
        float prePreTargetScale = _curTargetScale;
        _isPreMatched = _isMatched;
        
        float _matchedPrecentTemp =  (float)_activePoints.size()/_matcher.target_length;
        _isMatched = calcIsMatched(curScale,prePreTargetScale,_matchedPrecentTemp);
        
        _matchedPrecent = 0.0f;
        
        if (_isMatched)
        {
            _matchedPrecent = _matchedPrecentTemp;
            _curTargetScale = curScale;
        }
        else
        {
            _curTargetScale = 1.0f;
        }
        
        if (TIMEDEBUG)
        {
            printf("EVENT calcIsMatched curScale:%.2f preScale:%.2f matchPrecent:%.3f\n",curScale  ,prePreTargetScale  ,_matchedPrecent);
        }
        
        return _isMatched;
    }
    

} /* namespace CMT */
