#ifndef CMT_H
#define CMT_H

#include "common_cmt.h"
#include "Consensus.h"
#include "Fusion.h"
#include "Matcher.h"
#include "Tracker.h"
#include "MGPredictor.h"

//性能平衡系统
#include <opencv2/features2d/features2d.hpp>
#include "MGPerformanceAdapter.h"

#include "MGLearnT.h"


//因为类成员变量的命名和局部变量相同，发生几次难查的bug
//从17.2.24起，类成员变量用开头用下划线


using cv::FeatureDetector;
using cv::DescriptorExtractor;
using cv::RotatedRect;
using cv::Size2f;

namespace cmt
{
    
    class CMT
    {
    public:
        CMT();
        ~CMT();
        bool initialize( Mat& im_RGBA, const cv::Rect rect);
        //void processFrame(const Mat im_RGBA);
        
        void processFrame(Mat &orig_im_RGBA);
        void processFrame_inner(Mat orig_im_RGBA) ;
        void processFrameWithKeypointsByLearnMatcher(bool all,cv::Mat &im_gray,vector<KeyPoint> &keypoints, vector<Point2f> points_tracked, vector<int> classes_tracked);
        
        void reSizeMatcher(float scale);
        bool setMatchedParam(float curScale);
        bool processFrame(cv::Mat &orig_im_RGBA,cv::RotatedRect *pOutRot);
        
        //预测匹配
        void processFrameWithKeypoints(bool all,cv::Mat &im_gray,vector<KeyPoint> &keypoints, vector<Point2f> points_tracked, vector<int> classes_tracked);
        void track(const Mat &im_prev, const Mat &im_gray, const vector<Point2f> & points_prev,
                   vector<Point2f> & points_tracked, vector<unsigned char> & status,vector<int> &classes_tracked);
        
    public:
        
        Fusion _fusion;   // 数据融合器
        Matcher _matcher; // 特征匹配器
        Tracker _tracker; // 跟踪器
        Consensus _consensus;  // 一致检查器
        
        PerformanceAdapter _perfAdapter; //性能适配器
        Predictor _posPredictor;  //预测器
        
        ModelLearnMatcher learnMatcher; //学习器
        
        vector<Point2f> _activePoints; //public for visualization purposes 有效特征点
        RotatedRect _outRot;       //需要反算缩放输出
        
    public:
        //模型更新
        bool _isOpenLearn;
        MGLearnT *_pLearn_MGT_T1; //用原目标里的活动点创建的跟踪器
        MGLearnT *_pLearn_MGT_T2; //用原目标里的活动点创建的跟踪器
        Mat _curKPTDescriptors;
        
    public:
        
        //均值防抖动
        cv::Point2f pre_center1;
        cv::Point2f pre_center2;
        float pre_scale1;
        float pre_scale2;
        
        //判断跟踪结果
        bool  _isPreMatched;
        bool  _isMatched;
        float _matchedPrecent; //匹配百分比，衡量跟踪优劣
        
        float _curTargetScale;
        bool  _isCurProdictorMatched; //是否是预测成功
        
        cv::Rect _predictorRect;
        bool  useLearnMatcher;
        
        //帧率计算
        float _sumCalcTime;
        int  _calcTimeframeCount;
        list<float> _frameTimeList;
        
        float trackInstantaneousTimeConsuming;
        float track30AvgTimeConsuming;
        float track30AvgFrameRate;
        
        float trackCurFrameCost;
        
        //
        int _curFrameKeypointsCount;
        int _curPreditorFilterFrameKeypointsCount;
        int _curFilterByRDTDKPTCount;
        
        //重建匹配器
        Mat _origRGBImage;
        cv::Rect _origInitTargetRect;
        
        int _resizeMatcherFrameCount; //缩放了图片后，经过了多少帧，用于跟踪稳定之后再开启其他优化
        
        //debugmodel
        bool isDebugModel;
        bool _isDebugOpenPredictor;
        
        
        float _manualScale; //手动缩放参数
        bool _manualScaleChanged;
        
        Mat debugMat;
        Mat debugMatAllSize;
        
        
        //统计
        int tOkCount;
        int tFalseCount;
        
        vector<Point2f> testTPosArray;
        vector<int> testTResultArray;
        
        void pAAALog();
    protected:
        
        bool setup(const Mat grayImage, vector<KeyPoint> &keypoints,const cv::Rect targetRect);
    protected:
        cv::Ptr<FeatureDetector> detector;
        cv::Ptr<DescriptorExtractor> descriptor;
        
        Size2f size_initial;  // 初始大小
        cv::Rect _targetRect;  // 初始大小
        
        vector<int> _activeClasses;
        
        float theta;
        
        Mat im_prev;
        
        RotatedRect _innerRot; //内部计算用，未经过缩放
    };
    
    
} /* namespace CMT */

#endif /* end of include guard: CMT_H */
