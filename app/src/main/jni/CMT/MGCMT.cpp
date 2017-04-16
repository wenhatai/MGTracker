#include "MGCMT.h"

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



    CMT::CMT():_sumCalcTime(0),_calcTimeframeCount(0),_curTargetScale(1.0)
    {
        printf("EVENT CMT 创建 %p\n",this);

        _isDebugOpenPredictor = true;
        isDebugModel = false;
        _manualScaleChanged = false;
        useLearnMatcher = false;

        _isMatched = false;
        useLearnMatcher = false;
        _isPreMatched = false;

        _matchedPrecent = 0; //匹配百分比，衡量跟踪优劣

        _curTargetScale = 1;
        _isCurProdictorMatched = false; //是否是预测成功

        _curFrameKeypointsCount = 0;
        _curPreditorFilterFrameKeypointsCount = 0;
        _resizeMatcherFrameCount = 0;
        _curFilterByRDTDKPTCount = 0;

        //Initialize detector and descriptor 初始化检测器FAST和描述器BRISK
        #if CV_MAJOR_VERSION > 2
            descriptor = cv::BRISK::create();
        #else
            descriptor = new cv::BRISK;
        #endif

        _isOpenLearn = false;

        // _pLearn_MGT_T1 = new MGLearnT;
        // _pLearn_MGT_T2 = new MGLearnT;


    }

    CMT::~CMT()
    {
        printf("EVENT CMT 析构 %p\n",this);

        // delete _pLearn_MGT_T1;
        // delete _pLearn_MGT_T2;

    }

    bool CMT::initialize(const Mat im_RGBA, const cv::Rect initRect)
    {
        _resizeMatcherFrameCount = 0;

        _perfAdapter = PerformanceAdapter();
        _posPredictor = Predictor();
        if (TIMEDEBUG)
        {
            printf("EVENT CMT CMT::initialize: Rect:(%d,%d,%d,%d)\n",initRect.x,initRect.y,initRect.width,initRect.height);
        }

        //保存初始值
        _origRGBImage = im_RGBA;
        _origInitTargetRect = initRect;

        //Get initial keypoints in whole image and compute their descriptors
        vector<KeyPoint> keypoints;

//        //MGX_PERF_LOG_BEGIN_TIME(calcInitKeypointsAndOptimumPerfParam)
        double startCTime_calcInitKeypointsAndOptimumPerfParam= now_ms();

        //mgt 用性能适配器评估设备性能，并计算特征点
        cv::Rect rect; //有性能评估器缩放过的目标框
        Mat grayImage;   //缩放过的灰度图片
        _perfAdapter.calcInitKeypointsAndOptimumPerfParam(im_RGBA,initRect,grayImage,rect,keypoints);
        if (TIMEDEBUG)
        {
            printf("CMTTIME calcInitKeypointsAndOptimumPerfParam:%.3f,scale:%.3f,th:%d\n",(now_ms()-startCTime_calcInitKeypointsAndOptimumPerfParam)*1000.0/CLOCKS_PER_SEC,_perfAdapter._initScale,_perfAdapter._initFastThreshold);

            char log[100] = {0};
            sprintf(log, "初始化评估性能参数scale:%.3f,th:%d",_perfAdapter._initScale,_perfAdapter._initFastThreshold);
//            //MGX_PERF_LOG_APP("calcInitKeypointsAndOptimumPerfParam", log, calcInitKeypointsAndOptimumPerfParam, false);
        }

        //
        bool isTargetHasKeypointDesc = setup(grayImage, keypoints, rect);
        return isTargetHasKeypointDesc;
    }


    bool CMT::setup(const Mat grayImage, vector<KeyPoint> &keypoints,const cv::Rect targetRect)
    {
        _resizeMatcherFrameCount = 0;

        //存储跟踪区域的初始大小
        size_initial = targetRect.size();
        _targetRect = targetRect;

        //存储初始灰度图像
        im_prev = grayImage;

        //计算跟踪区域的中心位置
        Point2f center = Point2f(targetRect.x + targetRect.width/2.0, targetRect.y + targetRect.height/2.0);
        //初始化预测器
        _posPredictor.initialize(center);

        //分离出跟踪框和背景的关键点
        std::vector<cv::KeyPoint> targetKeypoints;
        std::vector<cv::KeyPoint> backgroundKeypoints;
        inoutRect(keypoints, targetRect, &targetKeypoints, &backgroundKeypoints);

        //计算跟踪框和背景的特征
        Mat targetDescs;
        Mat backgroundDescs;
        descriptor->compute(grayImage, targetKeypoints, targetDescs);

        //有些点无法计算特征
//        if (targetKeypoints.size() == 0)
//        {
//            return false;
//        }

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
        _consensus.initialize(normalizedPoints);

        //Create initial set of active keypoints 创建初始的活动点，即前景关键点的坐标
        _activePoints = targetPoints;
        _activeClasses = targetClasses;


        pre_center1 = cv::Point2f();
        pre_center2 = cv::Point2f();

        //初始化p1 自动更新跟踪器
//        _pLearn_MGT_T1->initialize(grayImage,keypoints,targetRect,_perfAdapter);
//        _pLearn_MGT_T2->initialize(grayImage,keypoints,targetRect,_perfAdapter);

        return true;
    }



    void CMT::reSizeMatcher(float scale)
    {
        _resizeMatcherFrameCount = 0;
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
        //初始化

        Mat grayImage;

        //用原图缩放图片
        cv::Size reSize = cv::Size(_origRGBImage.cols*scale,_origRGBImage.rows*scale);
        Mat reSizeImage_RGBA = cv::Mat(reSize,CV_8UC4);
        cv::resize(_origRGBImage, reSizeImage_RGBA,reSize);

        //转灰度
        cvtColor(reSizeImage_RGBA,grayImage,CV_RGB2GRAY);

        //Get initial keypoints in whole image and compute their descriptors
        vector<KeyPoint> keypoints;

        //缩放
        cv::Rect rect = scaleRect(_origInitTargetRect,scale);
        _perfAdapter.MG_FASTX(grayImage, keypoints, _perfAdapter._initFastThreshold);


        //
        _perfAdapter.calcTargetKeypointsResponseDistribution(keypoints, rect);

        vector<KeyPoint> keypointsFilte;
        _perfAdapter.keypointsFilterByRegionalDensityThresholdDistribution(keypoints,keypointsFilte,grayImage,_perfAdapter._initScale, _perfAdapter._initKPTRegionalDensity);

        //
        setup(grayImage, keypointsFilte, rect);

    }

    bool CMT::processFrame(cv::Mat &orig_im_RGBA,cv::RotatedRect *pOutRot)
        {
            processFrame(orig_im_RGBA);

            if (_isMatched)
            {
                *pOutRot = _outRot;
            }
            else
            {
                *pOutRot = cv::RotatedRect();
            }

            return _isMatched;
        }

    void CMT::processFrame(Mat &cur_orig_im_RGBA)
    {
        ++_resizeMatcherFrameCount;

        //debug 手动调整了缩放参数
        if (_manualScaleChanged && _perfAdapter._isManual)
        {
            float tempScale = _manualScale;
            tempScale = tempScale > 1.0?1.0:tempScale;
            tempScale = tempScale < 0.1?0.1:tempScale;

            _perfAdapter._initScale = tempScale;
            _manualScaleChanged = false;
            //调整了缩放尺寸，需要重建匹配器
            reSizeMatcher(tempScale);


            printf("EVENT calcRuningKeypoints perfAdapter.m_initScale:%.2f manualScale :%.2f tempScale: %.2f cur_orig_im_RGBA.w:%d cur_orig_im_RGBA.h:%d im_pre.w: %d im_pre.h: %d \n",_perfAdapter._initScale,_manualScale,tempScale,cur_orig_im_RGBA.cols, cur_orig_im_RGBA.rows,im_prev.cols,im_prev.rows);
        }

        double startCTime_all = now_ms();
        processFrame_inner(cur_orig_im_RGBA);

        float preFrameTime = (now_ms()- startCTime_all)/(float)CLOCKS_PER_SEC;


        //瞬时帧率计算，评估当前识别性能，反馈到性能适配器
        _frameTimeList.push_back(preFrameTime);

        _sumCalcTime += preFrameTime;
        ++_calcTimeframeCount;

        float frameRate = _calcTimeframeCount/_sumCalcTime;

        trackInstantaneousTimeConsuming = preFrameTime;
        track30AvgTimeConsuming = _sumCalcTime/_frameTimeList.size();
        track30AvgFrameRate = frameRate;
        trackCurFrameCost = preFrameTime*1000;

        //3帧平均帧率，越小越灵敏
        if (_calcTimeframeCount > 3)
        {
            _sumCalcTime -= (*_frameTimeList.begin());
            _frameTimeList.pop_front();
            --_calcTimeframeCount;
        }

        //对运行时进行动态性能评估，调整识别参数
        float preScale = _perfAdapter._initScale;
        _perfAdapter.calcBestThr(_matchedPrecent,frameRate,_matcher.target_length,_curFrameKeypointsCount);

        if (fabs(_perfAdapter._initScale - preScale) > 0.08)
        {
            //动态调整了缩放尺寸，需要重建匹配器
            reSizeMatcher(_perfAdapter._initScale);

        }

        if (TIMEDEBUG) {
            ALOG("EVENT processFrame matchPrecent:%.3f frameRate:%.2f\n",_matchedPrecent,frameRate);
        }

        //更新预测器
        //初始化预测器

        if (_isMatched)
        {
            _posPredictor.correct(_innerRot.center);
        }


        //测试跟踪轨迹
        //        if (isMatched) {
        //            ++tOkCount;
        //            testTPosArray.push_back(bb_rot.center);
        //        }
        //        else
        //        {
        //            ++tFalseCount;
        //            testTPosArray.push_back(Point2f(0,0));
        //        }
        //
        //
        //
        //        testTResultArray.push_back(isMatched);



    }

    static bool isPreRectOK(Rect r,Size s)
    {
        if (r.tl().x < 1 ||
            r.tl().y < 1 ||
            r.br().x > s.width - 1 ||
            r.br().y > s.height - 1 ||
            r.width == 0 ||
            r.height == 0
            )
        {
            return false;
        }

        return true;
    }


    //计算裁切上一帧的图区域
    static bool calcPredictROIRect(cv::Rect predictorRect,RotatedRect innerRot,cv::Size imageSize,cv::Rect &preROIRect)
    {
        bool isOK = true;

        cv::Point preTargetCenter(innerRot.center.x,innerRot.center.y);
        //用预测框把上一帧的图片裁切，上一帧的目标落在正中，计算位置
        preROIRect = cv::Rect(preTargetCenter.x - predictorRect.width/2, preTargetCenter.y - predictorRect.height/2,predictorRect.width, predictorRect.height);

        if (!isPreRectOK(preROIRect, imageSize))
        {

            isOK = false;

            //测试发现前一帧不把目标放到正中，跟踪成功率很低,很遗憾，这里省了很多时间
//            //框在中心点，超出图像了，换左上角
//            preROIRect = cv::Rect(innerRot.boundingRect().tl().x,innerRot.boundingRect().tl().y,predictorRect.width,predictorRect.height);
//
//            if (!isPreRectOK(preROIRect, imageSize))
//            {
//                //框在中心点，超出图像了，换右下
//                preROIRect = cv::Rect(innerRot.boundingRect().br().x - predictorRect.width,innerRot.boundingRect().br().y - predictorRect.height ,predictorRect.width,predictorRect.height);
//
//                if (!isPreRectOK(preROIRect, imageSize))
//                {
//                    //框在中心点，超出图像了，换左下角
//                    preROIRect = cv::Rect(innerRot.boundingRect().tl().x,innerRot.boundingRect().br().y -predictorRect.height,predictorRect.width,predictorRect.height);
//
//                    if (!isPreRectOK(preROIRect, imageSize))
//                    {
//                        //框在中心点，超出图像了，换右上
//                        preROIRect = cv::Rect(innerRot.boundingRect().br().x - predictorRect.width,innerRot.boundingRect().tl().y  ,predictorRect.width,predictorRect.height);
//
//                        if (!isPreRectOK(preROIRect, imageSize))
//                        {
//                           isOK = false;
//                        }
//                    }
//                }
//            }
        }

        return isOK;
    }

    void CMT::processFrame_inner(Mat curRGBImage)
    {

        if (isDebugModel)
        {
            debugMatAllSize = curRGBImage.clone();
            _pLearn_MGT_T1->_pDebugMatAllSize = &debugMatAllSize;
            _pLearn_MGT_T2->_pDebugMatAllSize = &debugMatAllSize;
        }

        double startCTime_all = now_ms();

        //MGX_PERF_LOG_BEGIN_TIME(detect)

        double startCTime_calcRuningKeypoints= now_ms();

        //计算当前图像的关键点
        vector<KeyPoint> keypoints;

        cv::Mat grayImage;
        _perfAdapter.calcRuningKeypoints( curRGBImage, grayImage, keypoints);

        long calcKpt =     AlgorithmComplexity(grayImage.cols,grayImage.rows,keypoints.size(),_matcher.matcher_length,_matcher.target_length);

        if (TIMEDEBUG)
        {
//            //MGX_PERF_LOG_APP("detect", "检测特征点", detect, false);
            printf("CMTTIME detect:%.3f keypoints.size:%lu\n",(now_ms()- startCTime_calcRuningKeypoints)*1000.0/CLOCKS_PER_SEC,keypoints.size());
            printf("EVENT calcKpt： %ld im_gray.rows:%d,im_gray.cols:%d, allCount:%ld,allCount:%d,fCount:%d\n",calcKpt,grayImage.rows,grayImage.cols, keypoints.size(),_matcher.matcher_length,_matcher.target_length);

            printf("EVENT calcRuningKeypoints manaulScale:%.2f cur_orig_im_RGBA.w:%d cur_orig_im_RGBA.h:%d im_pre.w: %d im_pre.h: %d im_cur.w:%d im_cur.h:%d\n",_perfAdapter._initScale,curRGBImage.cols, curRGBImage.rows,im_prev.cols,im_prev.rows,grayImage.cols,grayImage.rows);
        }

        _curFrameKeypointsCount = (int)keypoints.size();


         vector<KeyPoint> goodKeypoints;
        //预测匹配
        cv::Rect predictorRectTemp =  _posPredictor.calcPredictRect(_innerRot.size,cv::Size(im_prev.cols,im_prev.rows));

        bool preFrameIsMatched = _isMatched;
        _isMatched = false;
        _isCurProdictorMatched  = false;
        _matchedPrecent = 0.0;
        _predictorRect = cv::Rect();


        //预测匹配条件，中间插入了一些全检测帧

        while (preFrameIsMatched && _isDebugOpenPredictor)
        {
            //限制条件不符合
            if (!_posPredictor.canUse() || !_perfAdapter.canPerdictorTrack() || _resizeMatcherFrameCount < 3)
            {
                break;
            }

            //预测框无效
            if (predictorRectTemp.width == 0 || predictorRectTemp.height == 0)
            {
                break;
            }


            //bug fix进行预测追踪时，要先把一些成员变量 备份下来，不然预测跟踪会更改这个值，封装独立函数变量太多了，偷下懒
            vector<Point2f> preTrackBak_activePoints  = _activePoints;
            vector<int>     preTrackBak_activeClasses = _activeClasses;

            RotatedRect preTrackBak_outRot = _outRot;
            RotatedRect preTrackBak_innerRot = _innerRot;
            float preTrackBak_curTargetScale = _curTargetScale;



            _perfAdapter.perdictorTrackCount();

            //Track keypoints
            vector<Point2f> trackedPoints;
            vector<unsigned char> trackedPointsStatus;
            vector<int> trackedClasses;


            //用预测框把上一帧的图片裁切，上一帧的目标落在正中，计算位置
            cv::Rect preROIRect ;
            bool isPreROIOK = calcPredictROIRect(predictorRectTemp,_innerRot,cv::Size(im_prev.cols,im_prev.rows),preROIRect);

            if (isPreROIOK)
            {
                if (isDebugModel)
                {
                    float reScale = 1.0/_perfAdapter._initScale;
                    cv::Rect preROIRectDraw(preROIRect.tl()*reScale,preROIRect.br()*reScale);
                    rectangle(debugMatAllSize, preROIRectDraw ,cv::Scalar(0,0,0,255));
                }

                //把光流区域挖出来，计算特征点在子图像中的新坐标
                vector<Point2f> roiActivePoints;

                for (int i = 0  ; i < _activePoints.size(); i ++)
                {
                    Point2f offsetPT = _activePoints[i] - Point2f(preROIRect.tl().x,preROIRect.tl().y);
                    roiActivePoints.push_back(offsetPT);
                }

                vector<Point2f> roiTrackedPoints;
                track(im_prev(preROIRect), grayImage(predictorRectTemp), roiActivePoints, roiTrackedPoints, trackedPointsStatus,trackedClasses);

                //跟踪出来的坐标再反算回去
                for (int i = 0  ; i < roiTrackedPoints.size(); i ++)
                {
                    Point2f offsetPT = roiTrackedPoints[i] + Point2f(predictorRectTemp.tl().x,predictorRectTemp.tl().y);
                    trackedPoints.push_back(offsetPT);
                }

                if (TIMEDEBUG)
                {
                    printf("CMTTIME predictor roiTrackedPoints:%ld\n",trackedPoints.size());
                }

//                //对比测试
//                {
//
//                    vector<Point2f> points_tracked2;
//                    vector<unsigned char> status2;
//                    vector<int> classes_tracked2;
//
//                    track(im_prev, grayImage, _activePoints, points_tracked2, status2 ,classes_tracked2);
//
//                    if (trackedClasses.size() != classes_tracked2.size())
//                    {
//                        printf("CMTTIME predictor roiTrackedPoints:%ld alltrackCount:%ld\n",trackedPoints.size(),classes_tracked2.size());
//                    }
//
//                }
            }
            else
            {
                //预测框光流预测框不到一帧的跟踪框，用全图光流
                track(im_prev, grayImage, _activePoints, trackedPoints, trackedPointsStatus ,trackedClasses);
            }

            //过滤掉预测框之外的特征点
            vector<KeyPoint> predictorKeypoints;
            _posPredictor.keypointsFilter(keypoints, predictorKeypoints, predictorRectTemp);

            //区域密度阈值分布过滤
            _perfAdapter.keypointsFilterByRDTD(predictorKeypoints, goodKeypoints, grayImage);

            _curPreditorFilterFrameKeypointsCount = (int)predictorKeypoints.size();
            _curFilterByRDTDKPTCount = (int)goodKeypoints.size();

            //开始跟踪
            processFrameWithKeypoints(false, grayImage, goodKeypoints,  trackedPoints, trackedClasses);


            //检测跟踪结果
            _isCurProdictorMatched = _isMatched && _matchedPrecent > 0.30;

            //如果没有预测成功，恢复之前的变量
            if (!_isCurProdictorMatched)
            {
                //恢复预测跟踪之前的成员变量
                _activePoints = preTrackBak_activePoints;
                _activeClasses = preTrackBak_activeClasses;
                _outRot = preTrackBak_outRot;
                _innerRot = preTrackBak_innerRot ;
                _curTargetScale = preTrackBak_curTargetScale;

            }

            if (TIMEDEBUG) {
                printf("EVENT 预测检测%s _isMatched:%s _matchedPrecent:%.2f keypoints:%lu preKeypoints:%lu \n",_isCurProdictorMatched?"成功":"失败", _isMatched?"成功":"失败",_matchedPrecent,keypoints.size(),predictorKeypoints.size());
            }

            //输出预测框供调试用
            _predictorRect = scaleRect(predictorRectTemp,  1.0/ _perfAdapter._initScale);

            break;
        }//while

        //如果预测匹配结果不理想，用全图匹配
        if (!_isCurProdictorMatched)
        {
            //预测光流
            //Track keypoints
            vector<Point2f> points_tracked;
            vector<unsigned char> status;
            vector<int> classes_tracked;

            track(im_prev, grayImage, _activePoints, points_tracked, status,classes_tracked);

            //区域密度阈值分布过滤
            goodKeypoints.clear();
            _perfAdapter.keypointsFilterByRDTD(keypoints, goodKeypoints, grayImage);
            _curFilterByRDTDKPTCount = (int)goodKeypoints.size();

            processFrameWithKeypoints(true, grayImage, goodKeypoints,  points_tracked, classes_tracked);


        }

        if(_isOpenLearn)
        {
            //自动更新跟踪器跟踪
            //1 先跟踪,T0跟踪结果可信度不高，启用T1

            if(!_isMatched ||
               (_isMatched && _matchedPrecent < 0.3))
            {

                _pLearn_MGT_T1->processFrameWithKeypoints(true, grayImage, goodKeypoints,_curKPTDescriptors);

            }

            //learn mgt
            //2 再训练，T0跟踪可信度不错的情况下训练T1
            if(_isMatched && _matchedPrecent > 0.5 && _curTargetScale < 1.2)
            {
                cv::Rect  newRect = _innerRot.boundingRect();
               _pLearn_MGT_T1->update(grayImage,keypoints,newRect);

            }

            //3 结果选优
            if((_isMatched && _matchedPrecent < 0.3 && _pLearn_MGT_T1->_isMatched && _pLearn_MGT_T1->_matchedPrecent > 0.5) ||
                (!_isMatched  && _pLearn_MGT_T1->_isMatched && _pLearn_MGT_T1->_matchedPrecent > 0.3))
            {
                RotatedRect learnOutRot = _pLearn_MGT_T1->_outRot;
                //如果有尺寸缩放，则还原尺寸
                if (_perfAdapter._initScale < 1.0f)
                {
                    float reScale = 1.0/_perfAdapter._initScale;
                    learnOutRot = RotatedRect(_pLearn_MGT_T1->_outRot.center * reScale,  _pLearn_MGT_T1->_outRot.size * reScale, _pLearn_MGT_T1->_outRot.angle);
                }

                _outRot = learnOutRot;
                _innerRot = _pLearn_MGT_T1->_innerRot;
                _isMatched = _pLearn_MGT_T1->_isMatched;

                //如果采用了p0的结果，开始训练p1
//                if(_pLearn_MGT_T1->_matchedPrecent > 0.5)
//                {
//                    cv::Rect  newRect = _innerRot.boundingRect();
//
//                    vector<KeyPoint> goodKeypointsP1;
//                    _perfAdapter.keypointsFilterByRegionalDensityThresholdDistribution(keypoints, goodKeypointsP1, grayImage,_perfAdapter._initKPTRegionalDensity);
//
//                    _pLearn_MGT_T2->update(grayImage,goodKeypointsP1,newRect);
//
//                }
            }

            //如果p00,p0 都扑街了，启用p1
//            if (!_isMatched &&  !_pLearn_MGT_T1->_isMatched)
//            {
//                _pLearn_MGT_T2->processFrameWithKeypoints(true, grayImage, goodKeypoints,_curKPTDescriptors);
//
//
//                if (_pLearn_MGT_T2->_isMatched)
//                {
//                    //交换p1和p0
//                    _pLearn_MGT_T1->clear();
//                    MGLearnT * tempT = _pLearn_MGT_T1;
//                    _pLearn_MGT_T1 = _pLearn_MGT_T2;
//                    _pLearn_MGT_T2 = tempT;
//
//
//                    RotatedRect learnOutRot = _pLearn_MGT_T1->_outRot;
//                    //如果有尺寸缩放，则还原尺寸
//                    if (_perfAdapter._initScale < 1.0f)
//                    {
//                        float reScale = 1.0/_perfAdapter._initScale;
//                        learnOutRot = RotatedRect(_pLearn_MGT_T1->_outRot.center * reScale,  _pLearn_MGT_T1->_outRot.size * reScale, _pLearn_MGT_T1->_outRot.angle);
//                    }
//
//                    _outRot = learnOutRot;
//                    _innerRot = _pLearn_MGT_T1->_innerRot;
//                    _isMatched = _pLearn_MGT_T1->_isMatched;
//                }
//            }

        }

        //Remember current image 更新上一帧图像
        im_prev = grayImage;


    }


    void CMT::track(const Mat& im_prev1, const Mat& im_gray2, const vector<Point2f> & points_prev,
                    vector<Point2f> & points_tracked, vector<unsigned char> & status,vector<int> &classes_tracked)
    {

       // return;

//        //MGX_PERF_LOG_BEGIN_TIME(trackertrack)

        double startCTime_track  = now_ms();
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

//            //MGX_PERF_LOG_APP("track", log, trackertrack, false);

            printf("CMTTIME track:%.3f tCount:%ld imgsize(x:%d,y:%d) \n",(now_ms()-startCTime_track)*1000.0/CLOCKS_PER_SEC,classes_tracked.size(),im_prev1.cols,im_gray2.rows);
        }

    }


    void CMT::processFrameWithKeypoints(bool all,cv::Mat &im_gray,vector<KeyPoint> &keypoints, vector<Point2f> points_tracked, vector<int> classes_tracked)
    {
        if (keypoints.size() <= 0)
        {
            return;
        }

        double startCTime_all = now_ms();
//        //MGX_PERF_LOG_BEGIN_TIME(compute)
        double startCTime_compute = now_ms();


        //debug 画出特征点
        if (isDebugModel)
        {
            cvtColor(im_gray,debugMat,CV_GRAY2BGRA);

            for(size_t i = 0; i < keypoints.size(); i++)
            {
                circle(debugMat, keypoints[i].pt, 2, cv::Scalar(255,255,0,255));
                circle(debugMatAllSize, keypoints[i].pt*(1.0/_perfAdapter._initScale) , 2, cv::Scalar(255,255,0,255));
            }

        }


        // 计算当前图像特征点的描述
        Mat descriptors;
        descriptor->compute(im_gray, keypoints, descriptors);
        _curKPTDescriptors = descriptors;


        if (TIMEDEBUG) {
//            //MGX_PERF_LOG_APP("compute", "描述特征点", compute, false);
            printf("CMTTIME compute:%.3f\n",(now_ms()-startCTime_compute)*1000.0/CLOCKS_PER_SEC);
        }

        double startCTime = now_ms();
//        //MGX_PERF_LOG_BEGIN_TIME(matchGlobal)

        //在全局和之前的数据库匹配特征点，计算出匹配的特征点
        vector<Point2f> points_matched_global;
        vector<int> classes_matched_global;
        vector<int>  curkpts_classes_matched;
        _matcher.matchGlobal(keypoints, descriptors, points_matched_global, classes_matched_global,curkpts_classes_matched);


        if (TIMEDEBUG) {
//            //MGX_PERF_LOG_APP("matchGlobal", "匹配全局特征点", matchGlobal, false);
            printf("CMTTIME matchGlobal:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();


        //Fuse tracked and globally matched points
        //融合跟踪和匹配的点
        //MGX_PERF_LOG_BEGIN_TIME(preferFirst)
        vector<Point2f> points_fused;
        vector<int> classes_fused;

        _fusion.preferFirst(points_tracked, classes_tracked, points_matched_global, classes_matched_global,
                           points_fused, classes_fused);


        if (TIMEDEBUG) {
//            //MGX_PERF_LOG_APP("preferFirst", "描述特征点", preferFirst, false);
            printf("CMTTIME preferFirst:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();
        //MGX_PERF_LOG_BEGIN_TIME(estimateScaleRotation)

        // 估计旋转和缩放利用最终的融合点
        float scale = 1.0f;
        float rotation = 0;

        _consensus.estimateScaleRotation(points_fused, classes_fused, scale, rotation);

        if (TIMEDEBUG)
        {
//            //MGX_PERF_LOG_APP("estimateScaleRotation", "估计旋转和缩放", estimateScaleRotation, false);
            printf("CMTTIME estimateScaleRotation:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }

        startCTime = now_ms();
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

        //learn mgt
        //检查一致性淘汰了一部分特征点，同步当前全局匹配keypt

        {
            vector<int>  curkpts_classes_matched2 = curkpts_classes_matched;
            curkpts_classes_matched.clear();

            set<int> classes_inlier_set(classes_inlier.begin(),classes_inlier.end());

            for (int i = 0 ; i < curkpts_classes_matched2 .size(); ++i)
            {
                bool bF = classes_inlier_set.count(classes_matched_global[i]);

                if (bF)
                {
                    curkpts_classes_matched.push_back(curkpts_classes_matched2[i]);
                }
            }
        }


        if (TIMEDEBUG)
        {
//            //MGX_PERF_LOG_APP("preferFirst", "计算一致性", estimateScaleRotation, false);
            printf("CMTTIME findConsensus:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }

        //MGX_PERF_LOG_BEGIN_TIME(matchLocal)
        startCTime = now_ms();

        //Match keypoints locally 局部匹配
        vector<Point2f> points_matched_local;
        vector<int> classes_matched_local;
        vector<int>  curkpts_classes_matched_local;
        _matcher.matchLocal(keypoints, descriptors, center, scale, rotation, points_matched_local, classes_matched_local,curkpts_classes_matched_local);




        //        //learn mgt
        //        //局部匹配可能增加一部分特征点，同步当前全局匹配keypt
        //        {
        //            set<int> classes_temp_set(curkpts_classes_matched.begin(),curkpts_classes_matched.end());
        //
        //
        //            for (int i = 0 ; i < curkpts_classes_matched_local .size(); ++i)
        //            {
        //                bool bF = classes_temp_set.count(curkpts_classes_matched_local[i]);
        //
        //                if (!bF)
        //                {
        //                    curkpts_classes_matched.push_back(curkpts_classes_matched_local[i]);
        //                }
        //            }
        //        }

        if (TIMEDEBUG) {
//            //MGX_PERF_LOG_APP("matchLocal", "局部匹配", matchLocal, false);
            printf("CMTTIME matchLocal:%.3f \n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }

        //MGX_PERF_LOG_BEGIN_TIME(preferFirst2)
        startCTime = now_ms();


        // 融合局部匹配的点和inliers
        _fusion.preferFirst(points_matched_local, classes_matched_local, points_inlier, classes_inlier, _activePoints, _activeClasses);

        if (TIMEDEBUG)
        {
            printf("CMTTIME preferFirst:%.3f points_inlier:%lu points_matched_local:%lu points_active:%lu\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC,points_inlier.size(),points_matched_local.size(),_activePoints.size());
        }

//        //MGX_PERF_LOG_APP("preferFirst2", "融合局部匹配的点和inliers", preferFirst2, false);


        //计算匹配程度
        setMatchedParam(scale);

        if (!_isMatched)
        {
            return ;
        }


        startCTime = now_ms();

        // 计算出新的跟踪窗口
        _outRot = RotatedRect(center ,  size_initial * scale , rotation/CV_PI * 180);
        _innerRot = _outRot;

        //learn mgt
        //框框出来了，再次计算落在框内新出现的特征点
        if(_isMatched && _matchedPrecent > 0.5 && scale < 1.2)
        {
            cv::Rect newRect = _innerRot.boundingRect();

            vector<int> curTargetNewkeypoints;
            set<int> classes_temp_set(curkpts_classes_matched.begin(),curkpts_classes_matched.end());

            for (int i = 0; i < keypoints.size(); i++)
            {

                KeyPoint k = keypoints[i];
                Point2f pt = k.pt;

                if (pt.x > newRect.x && pt.y > newRect.y && pt.x < newRect.br().x && pt.y < newRect.br().y)
                {
                    if (classes_temp_set.count(i) <= 0)
                    {
                        curTargetNewkeypoints.push_back(i);;
                    }

                }
            }


            //learnMatcher.updateModel(keypoints, curkpts_classes_matched_local, classes_matched_local, curTargetNewkeypoints, descriptors,_outRot.size, bb_rot.center);



        }

        //均值解决跳动问题
        if (!isDebugModel && 0)
        {
            RotatedRect r_bb_rot;
            if (pre_center1.x != 0 && pre_center1.y != 0) {
                float scale2 = (scale + pre_scale1) * 0.5;
                r_bb_rot = RotatedRect(cv::Point2f((center.x + pre_center1.x) / 2.0,(center.y + pre_center1.y) / 2.0) , size_initial * scale2, rotation/CV_PI * 180);
            } else if (pre_center1.x != 0 && pre_center1.y != 0 && pre_center2.x != 0 && pre_center2.y != 0) {
                float scale3 = (scale + pre_scale1 + pre_scale2) / 3;
                r_bb_rot = RotatedRect(cv::Point2f((center.x + pre_center1.x + pre_center2.x) / 3.0,(center.y + pre_center1.y + pre_center2.y) / 3.0) , size_initial * scale3, rotation/CV_PI * 180);
            } else {
                r_bb_rot = RotatedRect(center,size_initial * scale, rotation/CV_PI * 180);
            }
            //前一个点
            if (pre_center1.x == 0 && pre_center1.y == 0) {
                pre_center1 = center;
                pre_scale1 = scale;
            } else {
                pre_center1 = center;
                pre_scale1 = scale;
                //前前一个点
                pre_center2 = pre_center1;
                pre_scale2 = pre_scale1;
            }
            _outRot = r_bb_rot;

        }


        if (TIMEDEBUG) {
            printf("CMTTIME %s 总耗时:%.3f \n",all?"全部检测成功":"预测检测成功",(now_ms()-startCTime_all)*1000.0/CLOCKS_PER_SEC);
        }
        //FILE_LOG(logDEBUG) << "CMT::processFrame() return";

        if (isDebugModel)
        {
            //最后活动的特征点
            for(size_t i = 0; i < _activePoints.size(); i++)
            {
                circle(debugMat, _activePoints[i], 2, cv::Scalar(255,0,0,255));
                circle(debugMatAllSize, _activePoints[i]*(1.0/_perfAdapter._initScale) , 2, cv::Scalar(255,0,0,255));
            }

            RotatedRect rect = _innerRot;
            Point2f vertices[4];
            rect.points(vertices);
            for (int i = 0; i < 4; i++)
            {
                //line(debugMat, vertices[i], vertices[(i+1)%4], cv::Scalar(0,255,0,255));

                line(debugMatAllSize, vertices[i]*(1.0/_perfAdapter._initScale), vertices[(i+1)%4]*(1.0/_perfAdapter._initScale), cv::Scalar(0,0,255,255));
            }

            //rectangle(debugMat, rect.boundingRect() ,cv::Scalar(0,255,0,255));
        }

        //如果有尺寸缩放，则还原尺寸
        if (_perfAdapter._initScale < 1.0f)
        {
            float reScale = 1.0/_perfAdapter._initScale;
            _outRot = RotatedRect(_outRot.center * reScale,  _outRot.size * reScale, _outRot.angle);
        }
    }


    void CMT::processFrameWithKeypointsByLearnMatcher(bool all,cv::Mat &im_gray,vector<KeyPoint> &keypoints, vector<Point2f> points_tracked, vector<int> classes_tracked)
    {
        if (keypoints.size() <= 0)
        {
            return;
        }

        double startCTime_all = now_ms();
        //MGX_PERF_LOG_BEGIN_TIME(compute)
        double startCTime_compute = now_ms();

        // 计算当前图像特征点的描述
        Mat descriptors;
        descriptor->compute(im_gray, keypoints, descriptors);

//        //MGX_PERF_LOG_APP("compute", "描述特征点", compute, false);
        if (TIMEDEBUG) {
            printf("CMTTIME compute:%.3f\n",(now_ms()-startCTime_compute)*1000.0/CLOCKS_PER_SEC);
        }

        double startCTime = now_ms();

        //MGX_PERF_LOG_BEGIN_TIME(matchGlobal)
        //Match keypoints globally 在全局和之前的数据库匹配特征点，计算出匹配的特征点
        vector<Point2f> points_matched_global;
        vector<int> classes_matched_global;
        vector<int>  curkpts_classes_matched;
        learnMatcher.matchGlobal(keypoints, descriptors, points_matched_global, classes_matched_global,curkpts_classes_matched);

//        //MGX_PERF_LOG_APP("matchGlobal", "匹配全局特征点", matchGlobal, false);

        if (TIMEDEBUG) {
            printf("CMTTIME matchGlobal:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();
        //FILE_LOG(logDEBUG) << points_matched_global.size() << " points matched globally.";


        //Fuse tracked and globally matched points
        //融合跟踪和匹配的点

        //MGX_PERF_LOG_BEGIN_TIME(preferFirst)
        vector<Point2f> points_fused;
        vector<int> classes_fused;

        _fusion.preferFirst(points_tracked, classes_tracked, points_matched_global, classes_matched_global,
                           points_fused, classes_fused);

//        //MGX_PERF_LOG_APP("preferFirst", "描述特征点", preferFirst, false);

        //MGX_PERF_LOG_BEGIN_TIME(estimateScaleRotation)
        if (TIMEDEBUG) {
            printf("CMTTIME preferFirst:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();
        //FILE_LOG(logDEBUG) << points_fused.size() << " points fused.";

        // 估计旋转和缩放利用最终的融合点
        //Estimate scale and rotation from the fused points
        float scale = 1.0f;
        float rotation = 0;

        _consensus.estimateScaleRotation(points_fused, classes_fused, scale, rotation);
//        //MGX_PERF_LOG_APP("estimateScaleRotation", "估计旋转和缩放", estimateScaleRotation, false);

        //MGX_PERF_LOG_BEGIN_TIME(findConsensus)
        if (TIMEDEBUG) {
            printf("CMTTIME estimateScaleRotation:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();
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

//        //MGX_PERF_LOG_APP("preferFirst", "计算一致性", estimateScaleRotation, false);

        //learn mgt
        //检查一致性淘汰了一部分特征点，同步当前全局匹配keypt

        {
            vector<int>  curkpts_classes_matched2 = curkpts_classes_matched;
            curkpts_classes_matched.clear();

            set<int> classes_inlier_set(classes_inlier.begin(),classes_inlier.end());

            for (int i = 0 ; i < curkpts_classes_matched2 .size(); ++i)
            {
                bool bF = classes_inlier_set.count(classes_matched_global[i]);

                if (bF)
                {
                    curkpts_classes_matched.push_back(curkpts_classes_matched[i]);
                }
            }
        }

        //MGX_PERF_LOG_BEGIN_TIME(matchLocal)
        if (TIMEDEBUG) {
            printf("CMTTIME findConsensus:%.3f\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();
        //FILE_LOG(logDEBUG) << points_inlier.size() << " inlier points.";
        //FILE_LOG(logDEBUG) << "center " << center;

        //Match keypoints locally 局部匹配
        vector<Point2f> points_matched_local;
        vector<int> classes_matched_local;
        vector<int>  curkpts_classes_matched_local;
        _matcher.matchLocal(keypoints, descriptors, center, scale, rotation, points_matched_local, classes_matched_local,curkpts_classes_matched_local);

//        //MGX_PERF_LOG_APP("matchLocal", "局部匹配", matchLocal, false);


        //        //learn mgt
        //        //局部匹配可能增加一部分特征点，同步当前全局匹配keypt
        //        {
        //            set<int> classes_temp_set(curkpts_classes_matched.begin(),curkpts_classes_matched.end());
        //
        //
        //            for (int i = 0 ; i < curkpts_classes_matched_local .size(); ++i)
        //            {
        //                bool bF = classes_temp_set.count(curkpts_classes_matched_local[i]);
        //
        //                if (!bF)
        //                {
        //                    curkpts_classes_matched.push_back(curkpts_classes_matched_local[i]);
        //                }
        //            }
        //        }


        //MGX_PERF_LOG_BEGIN_TIME(preferFirst2)
        if (TIMEDEBUG) {
            printf("CMTTIME matchLocal:%.3f \n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC);
        }
        startCTime = now_ms();
        //FILE_LOG(logDEBUG) << points_matched_local.size() << " points matched locally.";

        //Fuse locally matched points and inliers
        // 融合局部匹配的点和inliers
        _fusion.preferFirst(points_matched_local, classes_matched_local, points_inlier, classes_inlier, _activePoints, _activeClasses);
        //    points_active = points_fused;
        //    classes_active = classes_fused;
        if (TIMEDEBUG) {
            printf("CMTTIME preferFirst:%.3f points_inlier:%lu points_matched_local:%lu points_active:%lu\n",(now_ms()-startCTime)*1000.0/CLOCKS_PER_SEC,points_inlier.size(),points_matched_local.size(),_activePoints.size());
        }

//        //MGX_PERF_LOG_APP("preferFirst2", "融合局部匹配的点和inliers", preferFirst2, false);

        //计算匹配程度
        setMatchedParam(scale);


        startCTime = now_ms();
        //FILE_LOG(logDEBUG) << points_active.size() << " final fused points.";

        //TODO: Use theta to suppress result
        // 计算出新的跟踪窗口
        _outRot = RotatedRect(center ,  size_initial * scale , rotation/CV_PI * 180);
        _innerRot = _outRot;

        //learn mgt
        //框框出来了，再次计算落在框内新出现的特征点
        if(_isMatched && _matchedPrecent > 0.3 && scale < 1.5)
        {
            cv::Rect newRect = _outRot.boundingRect();

            vector<int> curTargetNewkeypoints;
            set<int> classes_temp_set(curkpts_classes_matched.begin(),curkpts_classes_matched.end());

            for (int i = 0; i < keypoints.size(); i++)
            {

                KeyPoint k = keypoints[i];
                Point2f pt = k.pt;

                if (pt.x > newRect.x && pt.y > newRect.y && pt.x < newRect.br().x && pt.y < newRect.br().y)
                {
                    if (classes_temp_set.count(i) <= 0)
                    {
                        curTargetNewkeypoints.push_back(i);;
                    }

                }
            }


           // learnMatcher.updateModel(keypoints, curkpts_classes_matched_local, classes_matched_local, curTargetNewkeypoints, descriptors,bb_rot.size, bb_rot.center);
        }

        //均值解决跳动问题
        if (!isDebugModel && 0)
        {
            RotatedRect r_bb_rot;
            if (pre_center1.x != 0 && pre_center1.y != 0) {
                float scale2 = (scale + pre_scale1) * 0.5;
                r_bb_rot = RotatedRect(cv::Point2f((center.x + pre_center1.x) / 2.0,(center.y + pre_center1.y) / 2.0) , size_initial * scale2, rotation/CV_PI * 180);
            } else if (pre_center1.x != 0 && pre_center1.y != 0 && pre_center2.x != 0 && pre_center2.y != 0) {
                float scale3 = (scale + pre_scale1 + pre_scale2) / 3;
                r_bb_rot = RotatedRect(cv::Point2f((center.x + pre_center1.x + pre_center2.x) / 3.0,(center.y + pre_center1.y + pre_center2.y) / 3.0) , size_initial * scale3, rotation/CV_PI * 180);
            } else {
                r_bb_rot = RotatedRect(center,size_initial * scale, rotation/CV_PI * 180);
            }
            //前一个点
            if (pre_center1.x == 0 && pre_center1.y == 0) {
                pre_center1 = center;
                pre_scale1 = scale;
            } else {
                pre_center1 = center;
                pre_scale1 = scale;
                //前前一个点
                pre_center2 = pre_center1;
                pre_scale2 = pre_scale1;
            }
            _outRot = r_bb_rot;
        }


        if (TIMEDEBUG) {
            printf("CMTTIME %s 总耗时:%.3f \n",all?"全部检测成功":"预测检测成功",(now_ms()-startCTime_all)*1000.0/CLOCKS_PER_SEC);
        }
        //FILE_LOG(logDEBUG) << "CMT::processFrame() return";

        //生成调试图片
        if (isDebugModel)
        {
            //最后活动的特征点
            for(size_t i = 0; i < _activePoints.size(); i++)
            {
                circle(debugMat, _activePoints[i], 2, cv::Scalar(255,0,0,255));
            }

            RotatedRect rect = _outRot;
            Point2f vertices[4];
            rect.points(vertices);
            for (int i = 0; i < 4; i++)
            {
                line(debugMat, vertices[i], vertices[(i+1)%4], cv::Scalar(0,255,0,255));
            }

            rectangle(debugMat, rect.boundingRect() ,cv::Scalar(0,255,0,255));
        }

        //如果有尺寸缩放，则还原尺寸
        if (_perfAdapter._initScale < 1.0f)
        {
            float reScale = 1.0/ _perfAdapter._initScale;
            _outRot = RotatedRect(_outRot.center * reScale,  _outRot.size * reScale, _outRot.angle);
        }
    }



    bool CMT::setMatchedParam(float curScale)
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


    static void pAALog(char* s , vector<Point2f>& array)
    {
        printf("CMTTIME=== track %s{\n",s);

        printf("X,");
        for (int i = 0; i < array.size(); ++i)
        {
            printf("%.2f,",array[i].x);
        }
        printf("\n  Y,");
        for (int i = 0; i < array.size(); ++i)
        {
            printf("%.2f,",array[i].y);
        }

        printf("\n  }");
    }

    void CMT::pAAALog()
    {
        return;
        printf("CMT======Result;tOkCount:%d tFalseCount:%d allCount:%d",tOkCount,tFalseCount,tOkCount + tFalseCount);

        pAALog("testTPosArray",testTPosArray);

        printf("result,");
        for (int i = 0; i < testTResultArray.size(); ++i)
        {
            printf("%d,",testTResultArray[i]);
        }

    }

} /* namespace CMT */
