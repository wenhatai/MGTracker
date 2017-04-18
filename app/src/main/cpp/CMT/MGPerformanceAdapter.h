//
//  MGPerformanceAdapter.h
//  MGPerformanceAdapter
//
//  Created by soapyang on 2017/2/17.
//  Copyright © 2016年 MG. All rights reserved.
//
#ifndef PerformanceAdapter_H

#define PerformanceAdapter_H

#include "common_cmt.h"
#include "Consensus.h"
#include "Fusion.h"
#include "Matcher.h"
#include "Tracker.h"

#include <opencv2/features2d/features2d.hpp>

using cv::FeatureDetector;
using cv::DescriptorExtractor;

using cv::RotatedRect;
using cv::Size2f;

namespace cmt
{

    //性能适配器
    //1 开始计算缩放量，如果特征点密度过大，先通过缩小图片整体降低密度到最佳匹配密度的两倍， 再通过【区域密度阈值分布过滤法】过滤密度太大的区域。
    //(大)输入图片密度 ----（缩小图片达到最佳密度值*2）----> MGT_TARGET_BEST_KP_PERCENT*2 ---(区域密度阈值分布过滤)--->MGT_TARGET_BEST_KP_PERCENT（小）
class PerformanceAdapter
{
public:
    PerformanceAdapter()  ;
 
public:
    
    int _initFastThreshold;       //目标最佳阈值
    int _runingFastThreshold;       //搜索图最佳阈值
    
    float _initScale;            //初始化匹配器时缩放值
    float _runingScale;          //运行时动态调整的缩放值，仅仅用户计算特征点的图片缩放
    float _runingScaleNeedReSetMatcher;          //运行时动态调整的缩放值,需要重建匹配器
    
    float _initKPTRegionalDensity;         //图片特征点数量上限，通过 区域密度阈值分布过滤法把特征点减少到的上限，为初始化时限制值
    float _runingKPTRegionalDensity;      //运行时动态调整的图片特征点密度上限，通过 区域密度阈值分布过滤法把特征点减少到的上限，根据检测耗时和跟踪可信度动态反馈调整此参数

    list<pair<int,int> > _targetKeypontsDistributionRange;
    vector<int> _targetKeypontsDistributionCount;
    
    //debug 手动，关闭自动
    bool _isManual;
    float _curFrameRate;
    int _perTrackCount;
    
    bool _isDebugOpenRDTD;
    
    
protected:
    
    cv::Ptr<FeatureDetector> detector;

public:
    void calcInitKeypointsAndOptimumPerfParam(const Mat &im_gray, const cv::Rect rect,Mat &reSizeGrayImage, cv::Rect &reSizeRect,vector<KeyPoint> &keypoints);
    
    void calcRuningKeypoints(const Mat &im_gray,Mat &reSizeGrayImage, vector<KeyPoint> &keypoints);
    
    void calcBestThr(float matchPrecent,float frameRate,int targetCount,int newFrameTCount);
    
    
    bool canPerdictorTrack();
    void perdictorTrackCount();

    
    void detectKeypoints(const Mat &im_gray, vector<KeyPoint>& keypoints,int threshold,const cv::Rect origRect,const cv::Rect reSizeRect,long &calcKPCount,float &kpPrecent,int &fCount,int &bCount);
    void MG_FASTX(const Mat &im_gray, vector<KeyPoint>& keypoints,int threshold);
    
    void keypointsFilterByRDTD(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,const Mat &reSizeGrayImage);
    
public:
    
    void calcInitKeypointsAndOptimumPerfParamInner(const Mat &im_RGBA, const cv::Rect rect,Mat &reSizeGrayImage, cv::Rect &reSizeRect,vector<KeyPoint> &keypoints);
    void calcTargetKeypointsResponseDistribution(const vector<KeyPoint> &keypoints,cv::Rect rect);
    void keypointsFilterByRegionalDensityThresholdDistribution(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,const Mat &reSizeGrayImage,float imageScale,float dstMaxKPTRegionalDensity);

    

};
    

} /* namespace PerformanceAdapter_H */

#endif /* end of include guard: PerformanceAdapter_H */
