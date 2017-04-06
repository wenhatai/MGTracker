//
//  MGPerformanceAdapter.cpp
//  MGPerformanceAdapter
//
//  Created by soapyang on 2017/2/17.
//  Copyright © 2016年 MG. All rights reserved.
//
#include "MGPerformanceAdapter.h"
#include "MGCMT.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>


//10 fast 默认阈值
const float MGT_DEFAULT_FAST_THRESHOLD = (10.0f);

const int MGT_BENCHMARK_CAL_ABILITY_IPHONE5S = 1606282;
const int MGT_BENCHMARK_CAL_ABILITY_IPHONE6 = 1606282;
const int MGT_BENCHMARK_CAL_ABILITY_IPHONE6S = MGT_BENCHMARK_CAL_ABILITY_IPHONE6*2;//基准以15帧 60毫秒可以计算的特征点数量 以  AlgorithmComplexity函数来估计.
const int MGT_BENCHMARK_CAL_ABILITY_IPHONE7 = MGT_BENCHMARK_CAL_ABILITY_IPHONE6S*2;

const int MGT_BENCHMARK_CAL_ABILITY = MGT_BENCHMARK_CAL_ABILITY_IPHONE6; //基准100毫秒可以计算的特征点数量 以  AlgorithmComplexity函数来估计.

//尺寸缩放相关系数
const float MGT_TARGET_BEST_KP_PERCENT = (100.0/(100.0*100.0)); //跟踪框最大特征点密度 100*100 里最多100个特征点
const float MGT_TARGET_MIN_KP_PERCENT = (10.0/(100.0*100.0)); //跟踪框最小特征点密度 100*100 里最少10个特征点

const int MGT_TARGET_MIN_KP_COUNT = (10); //跟踪框最小特征点密度 100*100 里最少10个特征点

const float MGT_IMAGE_MIN_SCALE = 0.3; // 图片尺寸最小缩放值
const float MGT_IMAGE_SCALE_STEP = 0.1; //图片缩小步长

const float MGT_IMAGE_FAST_THR_STEP = 5; //图片特征点阈值步长
const float MGT_IMAGE_FAST_THR_MIN = 5; //图片特征点阈值最小值
const float MGT_IMAGE_FAST_THR_MAX = 20; //图片特征点阈值最大值


namespace cmt {
    
    static void CalcClassWithTargetRect(const vector<KeyPoint> &keypoints,const cv::Rect rect,int &fCount,int &bCount)
    {
        for (size_t i = 0; i < keypoints.size(); i++)
        {
            KeyPoint k = keypoints[i];
            Point2f pt = k.pt;
            
            if (pt.x > rect.x && pt.y > rect.y && pt.x < rect.br().x && pt.y < rect.br().y)
            {
                fCount++;
            }
            else
            {
                bCount++;
            }
            
        }
    }
    
    //
    
    PerformanceAdapter::PerformanceAdapter()
    {
    #if CV_MAJOR_VERSION > 2
        detector = cv::FastFeatureDetector::create();
    #else
        detector = new cv::FastFeatureDetector;
    #endif
        _initScale = 1.0;
        _runingScale = 1.0;
        
        _isManual = false;
        _isDebugOpenRDTD = true;
        
        _initFastThreshold = MGT_DEFAULT_FAST_THRESHOLD;
        _runingFastThreshold = MGT_DEFAULT_FAST_THRESHOLD;
        
        _curFrameRate = 0;
        _perTrackCount = 0;
        
        _initKPTRegionalDensity = MGT_TARGET_BEST_KP_PERCENT;
        _runingKPTRegionalDensity = _initKPTRegionalDensity;
        
    }
    
    
    //初始第一帧时确定缩放级别
    void PerformanceAdapter::calcInitKeypointsAndOptimumPerfParam(const Mat &im_RGBA, const cv::Rect rect,Mat &reSizeGrayImage, cv::Rect &reSizeRect,vector<KeyPoint> &keypoints)
    {
        keypoints.clear();
        vector<KeyPoint> keypointsTemp;
        calcInitKeypointsAndOptimumPerfParamInner(im_RGBA,rect,reSizeGrayImage,reSizeRect,keypointsTemp);
        //
        
        calcTargetKeypointsResponseDistribution(keypointsTemp, reSizeRect);
        keypointsFilterByRegionalDensityThresholdDistribution(keypointsTemp,keypoints,reSizeGrayImage, _initScale,_initKPTRegionalDensity);
        
    }
    //初始第一帧时确定缩放级别
    void PerformanceAdapter::calcInitKeypointsAndOptimumPerfParamInner(const Mat &im_RGBA, const cv::Rect rect,Mat &reSizeGrayImage, cv::Rect &reSizeRect,vector<KeyPoint> &keypoints)
    {
        int  nInitPerfFastThreshold = _initFastThreshold;
        
        Mat img_gray_s1;
        cvtColor(im_RGBA,img_gray_s1,CV_RGB2GRAY);
        
        reSizeGrayImage = img_gray_s1;
        reSizeRect = rect;
        
        //计算量
        long calcKPCount = 0;
        //评估跟踪框内特征点密度，计算图片缩放值
        float kpPrecent = 0;
        
        int fCount = 0;
        int bCount = 0;
        
        //计算全部的特征点，评估特征点规模
        detectKeypoints(reSizeGrayImage, keypoints, nInitPerfFastThreshold,rect, reSizeRect, calcKPCount, kpPrecent,fCount,bCount);
        
        
        //debug 手动模式，直接退出
        if (_isManual)
        {
            return;
        }
        
        //1 检查计算量：如果计算量已经小于基准，不在计算优化参数
        if (calcKPCount < MGT_BENCHMARK_CAL_ABILITY)
        {
            printf("EVENT calcKPCount < MGT_BENCHMARK_CAL_ABILITY calcKPCount:%lu m_initScale:%.2f,m_nPerfFastThreshold:%d\n",calcKPCount,_initScale,_runingFastThreshold);
            return;
        }
        
        //1 开始计算缩放量，如果特征点密度过大，先通过缩小图片整体降低密度到最佳匹配密度的两倍， 再通过【区域密度阈值分布过滤法】过滤密度太大的区域。
        //(大)输入图片密度 ----（缩小图片达到最佳密度值*2）----> MGT_TARGET_BEST_KP_PERCENT*2 ---(区域密度阈值分布过滤)--->MGT_TARGET_BEST_KP_PERCENT（小）
        float scale = 1.0f;
        
        int maxCalcCount = 20;
        while (true)
        {
            float preScale = scale;
            const float step = MGT_IMAGE_SCALE_STEP;
            scale -= step;
            scale = scale < 0.2?0.2:scale;
            
            //缩放图片
            cv::Size reSize = cv::Size(im_RGBA.cols*scale,im_RGBA.rows*scale);
            Mat reSizeImage_RGBA = cv::Mat(reSize,CV_8UC4);
            cv::resize(im_RGBA, reSizeImage_RGBA,reSize);
            
            //转灰度
            cvtColor(reSizeImage_RGBA,reSizeGrayImage,CV_RGB2GRAY);
            
            //缩放目标框
            reSizeRect = scaleRect(rect,scale);
            
            //计算特征
            detectKeypoints(reSizeGrayImage, keypoints, nInitPerfFastThreshold, rect,reSizeRect, calcKPCount, kpPrecent,fCount,bCount);
            
            
            //先判断是否缩小的太厉害，比最佳密度两倍还小，回退用上一个缩放值  /判断计算量已经是否小于基准值
            if ((kpPrecent < MGT_TARGET_BEST_KP_PERCENT || (calcKPCount < MGT_BENCHMARK_CAL_ABILITY )) && fCount < 300)
            {
                //小于基准值就打行日志
                if (calcKPCount < MGT_BENCHMARK_CAL_ABILITY )
                {
                    printf("EVENT 缩小尺寸 calcKPCount < MGT_BENCHMARK_CAL_ABILITY calcKPCount:%lu m_initScale:%.2f,_runingFastThreshold:%d\n",calcKPCount,_initScale,_runingFastThreshold);
                }
                
                scale = preScale;
                //缩放图片
                cv::Size reSize = cv::Size(im_RGBA.cols*scale,im_RGBA.rows*scale);
                Mat reSizeImage_RGBA = cv::Mat(reSize,CV_8UC4);
                cv::resize(im_RGBA, reSizeImage_RGBA,reSize);
                
                cvtColor(reSizeImage_RGBA,reSizeGrayImage,CV_RGB2GRAY);
                
                //缩放目标框
                reSizeRect = scaleRect(rect,scale);
                
                //计算特征
                detectKeypoints(reSizeGrayImage, keypoints, nInitPerfFastThreshold,rect, reSizeRect, calcKPCount, kpPrecent,fCount,bCount);
                
                break;
            }
            
            if (--maxCalcCount <= 0)
            {
                break;
            }
        }
        
        
        //计算缩放完成
        _initScale = scale;
        
        //计算密度范围
        
        //2 减低背景特征点值,把目标框放大3倍，让目标落在正中，去掉框外的背景特征点
        do
        {
            if (calcKPCount < MGT_BENCHMARK_CAL_ABILITY )
            {
                break;
            }
            
            
            //目标太小，没有背景容易匹配到错误目标
            if (reSizeRect.size().width*reSizeRect.size().height < 100*100)
            {
                break;
            }
            
            int scaleFilterRect = 4;
            
            if (reSizeRect.size().width*reSizeRect.size().height < 200*200)
            {
                scaleFilterRect = 6;
            }
            
            cv::Size filterRectSize = reSizeRect.size()*scaleFilterRect;
            cv::Point targetCenter((reSizeRect.tl() + reSizeRect.br()) *0.5);
            cv::Rect targetROIRect = cv::Rect(targetCenter.x - filterRectSize.width/2, targetCenter.y - filterRectSize.height/2,filterRectSize.width, filterRectSize.height);
            
            vector<cv::KeyPoint> targetKeypointsAndTargetX3backgroundKeypoints;
            
            for (size_t i = 0; i < keypoints.size(); i++)
            {
                KeyPoint k = keypoints[i];
                Point2f pt = k.pt;
                
                if (targetROIRect.contains(pt))
                {
                    targetKeypointsAndTargetX3backgroundKeypoints.push_back(k);
                }
            }
            
            keypoints = targetKeypointsAndTargetX3backgroundKeypoints;
            
        }while (false);
        
    }
    
    
    void PerformanceAdapter::detectKeypoints(const Mat &im_gray, vector<KeyPoint>& keypoints,int threshold,const cv::Rect origRect,const cv::Rect reSizeRect,long &calcKPCount,float &kpPrecent,int &fCount,int &bCount)
    {
        //计算特征
        keypoints.clear();
        MG_FASTX(im_gray, keypoints, threshold);
        
        size_t allCount = keypoints.size();
        
        fCount = 0;
        bCount = 0;
        CalcClassWithTargetRect(keypoints, reSizeRect, fCount, bCount);
        
        //计算量
        calcKPCount = AlgorithmComplexity(im_gray.rows,im_gray.cols, allCount,allCount,fCount);
        //***重要：评估跟踪框内特征点密度,这里计算密度必须用原始的特征框大小，不能用缩放过的特征框，缩放图片只是计算特征点的临时过程，特征点要映射到原图上去
        long fArea = origRect.width *  origRect.height;
        kpPrecent = (float)fCount/(float)fArea;
        
        if (TIMEDEBUG) {
            printf("EVENT PerformanceAdapter： %d im_gray.rows:%d,im_gray.cols:%d, allCount:%d,allCount:%d,fCount:%d\n",calcKPCount,im_gray.rows,im_gray.cols, allCount,allCount,fCount);
        }
    }
    
    
    //计算区域内特征点阈值分布,过滤特征点时优先过滤分布低的
    void  PerformanceAdapter::calcTargetKeypointsResponseDistribution(const vector<KeyPoint> &keypoints,cv::Rect rect)
    {
        vector<pair<int,int> > rangeArray;
        pair<int,int> range1(0,20);
        pair<int,int> range2(20,50);
        pair<int,int> range3(50,255);
        
        rangeArray.push_back(range1);
        rangeArray.push_back(range2);
        rangeArray.push_back(range3);
        
        vector<int> rangeCountArray(rangeArray.size());
        
        //计算每个范围内的特征点分布
        for (int i = 0 ; i < keypoints.size(); ++i)
        {
            //不在框里的不计算
            
            if((rect.width > 0 && rect.height > 0) && !(rect.contains(keypoints[i].pt)))
            {
                continue;
            }
            
            float response = keypoints[i].response;
            
            for (int j = 0 ; j < rangeArray.size(); ++j)
            {
                if (response > rangeArray[j].first && response <= rangeArray[j].second)
                {
                    rangeCountArray[j] =  rangeCountArray[j] + 1;
                }
            }
        }
        
        
        //对分布区域有低到高排序
        list<int> sortRangeIndexList;
        
        for (int i = 0; i < rangeCountArray.size() ; ++i)
        {
            list<int>::iterator insertIter = sortRangeIndexList.end();
            for (list<int>::iterator iter = sortRangeIndexList.begin(); iter != sortRangeIndexList.end(); ++iter)
            {
                if (rangeCountArray[i] < rangeCountArray[*iter])
                {
                    insertIter = iter;
                    break;
                }
            }
            
            sortRangeIndexList.insert(insertIter, i);
        }
        
        
        _targetKeypontsDistributionRange.clear();
        _targetKeypontsDistributionCount.clear();
        
        for (list<int>::iterator iter = sortRangeIndexList.begin(); iter != sortRangeIndexList.end(); ++iter)
        {
            _targetKeypontsDistributionRange.push_back(rangeArray[*iter]);
            _targetKeypontsDistributionCount.push_back(rangeCountArray[*iter]);
            
            if (TIMEDEBUG) {
                printf("EVENT calcDistribution range(%d,%d) count:%d \n",rangeArray[*iter].first,rangeArray[*iter].second,rangeCountArray[*iter]);
            }
        }
        
        
    }
    
    static bool rectIsContainsPT(const cv::Rect &r,const cv::Point2f &pt)
    {
        bool isContains = (r.x <= pt.x && pt.x < r.x + r.width && r.y <= pt.y && pt.y < r.y + r.height);
        return isContains;
    }
    
    void PerformanceAdapter::keypointsFilterByRegionalDensityThresholdDistribution(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,const Mat &reSizeGrayImage,float imageScale,float runingDstKPTRegionalDensity)
    {
        //
        
        if (!_isDebugOpenRDTD) {
            dstKeypoints = srcKeypoints;
            return;
        }
        
        //
        float dstKPTRegionalDensity = runingDstKPTRegionalDensity;
        clock_t startCTime_all = clock();
        
        dstKeypoints.clear();
        
        cv::Size imageSize(reSizeGrayImage.cols,reSizeGrayImage.rows);
        vector<cv::Rect> rectArray;
        vector<vector<KeyPoint>> rectKeypointsArray;
        
        const float c_rectWH = 100;
        const int c_rectWHSp = c_rectWH*imageScale;
        
        //计算区域表
        for (int w = 0; w < imageSize.width; w = w + c_rectWHSp)
        {
            for (int h = 0; h < imageSize.height; h = h + c_rectWHSp)
            {
                cv::Rect r(w,h,c_rectWHSp,c_rectWHSp);
                rectArray.push_back(r);
            }
        }
        
        
        
        //把特征点分到区域表里
        rectKeypointsArray.resize(rectArray.size());
        
        for (int i = 0; i < srcKeypoints.size(); ++i)
        {
            for (int j = 0; j < rectArray.size(); ++j)
            {
                // if (rectArray[j].contains(srcKeypoints[i].pt))
                if ( rectIsContainsPT(rectArray[j],srcKeypoints[i].pt) )
                {
                    rectKeypointsArray[j].push_back(srcKeypoints[i]);
                }
            }
        }
        
        //对密度区域进行排序
        list<vector<KeyPoint>*> sortList;
        for (int i = 0; i < rectKeypointsArray.size() ; ++i)
        {
            //如何区域密度已经小于等于最佳密度了，不用进行排序过滤,直接加入输出数组
            if (rectKeypointsArray[i].size()/(float)(c_rectWH*c_rectWH) <= dstKPTRegionalDensity)
            {
                dstKeypoints.insert(dstKeypoints.end(), rectKeypointsArray[i].begin(),rectKeypointsArray[i].end());
                continue;
            }
            
            list<vector<KeyPoint>*>::iterator insertIter = sortList.end();
            for (list<vector<KeyPoint>*>::iterator iter = sortList.begin(); iter != sortList.end(); ++iter)
            {
                if (rectKeypointsArray[i].size() >= (*iter)->size())
                {
                    insertIter = iter;
                    break;
                }
            }
            
            sortList.insert(insertIter, &rectKeypointsArray[i]);
        }
        
        //对高密度区域抽样过滤
        int delKPTCount = 0;
        //从高到底开始处理
        for (list<vector<KeyPoint>*>::iterator iter = sortList.begin(); iter != sortList.end(); ++iter)
        {
            vector<KeyPoint>& rectKPTArray= *(*iter);
            vector<KeyPoint> newRectKPTArray;
            
            if (rectKPTArray.size()/(float)(c_rectWH*c_rectWH) > dstKPTRegionalDensity)
            {
                //把每个区域中的特征点在抽取到按响应区分的三个篮子里
                vector<vector<KeyPoint> > respRangeKeypointsArray(_targetKeypontsDistributionRange.size());
                
                for (int i = 0; i < rectKPTArray.size(); ++i)
                {
                    float response = rectKPTArray[i].response;
                    
                    int ir = 0;
                    for ( list<pair<int,int> >::iterator iter = _targetKeypontsDistributionRange.begin(); iter != _targetKeypontsDistributionRange.end();++iter)
                    {
                        pair<int,int> range = *iter;
                        
                        if (response > range.first && response <= range.second)
                        {
                            respRangeKeypointsArray[ir].push_back(rectKPTArray[i]);
                            break;
                        }
                        
                        ++ir;
                    }
                }
                
                //先对和跟踪框的特征最不相似
                int bestCount = (c_rectWH*c_rectWH)*dstKPTRegionalDensity;
                
                
                for (int ik = respRangeKeypointsArray.size() - 1; ik >= 0; --ik)
                {
                    int respRangeKPTCount = respRangeKeypointsArray[ik].size();
                    int willCount = newRectKPTArray.size() + respRangeKPTCount;
                    if (willCount >  bestCount)
                    {
                        //全插入就多了，看看需要插入多少，就插入多少个
                        int needAddCount = bestCount - newRectKPTArray.size();

                        //小于最佳密度了，需要加回来
                        //方案1 直接从头加入，会造成特征点分成明显的一条一条的缺失
                        //newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].begin() + needAddCount);
                        
//方案二 均匀采样，这个方案有个很隐蔽又很严重的陷阱， 每次计算出的特征点可能不一样，想象，从100个里随机取25个，两次取得可能完全不相同，导致目标模型里取得点和后面帧取得点根本不是一个，那么用KNN匹配也匹配不到。
//                        for (float ii = 0; ii < respRangeKPTCount; ii = ii + (float)respRangeKPTCount/needAddCount)
//                        {
//                            newRectKPTArray.push_back(respRangeKeypointsArray[ik][(int)ii]);
//                        }
                        
                        //最终的方案3 对特征点按响应分数排序，取响应最强的 取top N
                        for (int tn = 0; tn < needAddCount ; ++tn)
                        {
                            for (int irr = tn; irr < respRangeKeypointsArray[ik].size() ; ++irr)
                            {
                                KeyPoint &maxKPT = respRangeKeypointsArray[ik][tn];
                                KeyPoint &kpt = respRangeKeypointsArray[ik][irr];
                                
                                if (kpt.response > maxKPT.response)
                                {
                                    KeyPoint kptTemp = respRangeKeypointsArray[ik][tn];
                                    respRangeKeypointsArray[ik][tn] = respRangeKeypointsArray[ik][irr];
                                    respRangeKeypointsArray[ik][irr] = kptTemp;
                                    
                                }
                            }
                        }
                        
                        newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].begin() + needAddCount);
                    
                        break;
                        
                    }
                    else if (willCount <  bestCount)
                    {
                        //把这个范围的全插入了，还小于最佳值，继续走循环看看下一个响应范围的吧
                        newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].end());
                    }
                    else if (willCount ==  bestCount)
                    {
                        //so good
                        newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].end());
                        break;
                        
                    }
                    
                }
                
            }
            else
            {
                //理论上不会进这里
                newRectKPTArray = rectKPTArray;
            }
            
            //
            delKPTCount = delKPTCount + rectKPTArray.size() - newRectKPTArray.size();
            dstKeypoints.insert(dstKeypoints.end(), newRectKPTArray.begin(),newRectKPTArray.end());
            
        }
        
        //区域均值化后，特征点数量还是多，就只能随机抽样的
        
        if (TIMEDEBUG)
        {
            printf("CMTTIME 区域密度响应分布过滤法耗时:%.3f 过滤：%d 过滤前：%lu 过滤后:%lu  \n",(clock()-startCTime_all)*1000.0/CLOCKS_PER_SEC,delKPTCount,srcKeypoints.size(),dstKeypoints.size());
        }
        
        
    }
    
    
    
//    void PerformanceAdapter::keypointsFilterByRegionalDensityThresholdDistribution(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,const Mat &reSizeGrayImage,float runingDstKPTRegionalDensity)
//    {
//        //
//        
//        if (!_isDebugOpenRDTD) {
//            dstKeypoints = srcKeypoints;
//            return;
//        }
//        
//        //
//        float dstKPTRegionalDensity = runingDstKPTRegionalDensity;
//        clock_t startCTime_all = clock();
//        
//        dstKeypoints.clear();
//        
//        cv::Size imageSize(reSizeGrayImage.cols,reSizeGrayImage.rows);
//        vector<cv::Rect> rectArray;
//        vector<vector<KeyPoint>> rectKeypointsArray;
//        
//        const int c_rectWH = 100;
//        
//        //计算区域表
//        for (int w = 0; w < imageSize.width; w = w + c_rectWH)
//        {
//            for (int h = 0; h < imageSize.height; h = h + c_rectWH)
//            {
//                cv::Rect r(w,h,c_rectWH,c_rectWH);
//                rectArray.push_back(r);
//            }
//        }
//        
//        
//        
//        //把特征点分到区域表里
//        rectKeypointsArray.resize(rectArray.size());
//        
//        for (int i = 0; i < srcKeypoints.size(); ++i)
//        {
//            for (int j = 0; j < rectArray.size(); ++j)
//            {
//                // if (rectArray[j].contains(srcKeypoints[i].pt))
//                if ( rectIsContainsPT(rectArray[j],srcKeypoints[i].pt) )
//                {
//                    rectKeypointsArray[j].push_back(srcKeypoints[i]);
//                }
//            }
//        }
//        
//        //对密度区域进行排序
//        list<vector<KeyPoint>*> sortList;
//        for (int i = 0; i < rectKeypointsArray.size() ; ++i)
//        {
//            //如何区域密度已经小于等于最佳密度了，不用进行排序过滤,直接加入输出数组
//            if (rectKeypointsArray[i].size()/(float)(c_rectWH*c_rectWH) <= dstKPTRegionalDensity)
//            {
//                dstKeypoints.insert(dstKeypoints.end(), rectKeypointsArray[i].begin(),rectKeypointsArray[i].end());
//                continue;
//            }
//            
//            list<vector<KeyPoint>*>::iterator insertIter = sortList.end();
//            for (list<vector<KeyPoint>*>::iterator iter = sortList.begin(); iter != sortList.end(); ++iter)
//            {
//                if (rectKeypointsArray[i].size() >= (*iter)->size())
//                {
//                    insertIter = iter;
//                    break;
//                }
//            }
//            
//            sortList.insert(insertIter, &rectKeypointsArray[i]);
//        }
//        
//        //对高密度区域抽样过滤
//        int delKPTCount = 0;
//        //从高到底开始处理
//        for (list<vector<KeyPoint>*>::iterator iter = sortList.begin(); iter != sortList.end(); ++iter)
//        {
//            vector<KeyPoint>& rectKPTArray= *(*iter);
//            vector<KeyPoint> newRectKPTArray;
//            
//            if (rectKPTArray.size()/(float)(c_rectWH*c_rectWH) > dstKPTRegionalDensity)
//            {
//                //把每个区域中的特征点在抽取到按响应区分的三个篮子里
//                vector<vector<KeyPoint> > respRangeKeypointsArray(_targetKeypontsDistributionRange.size());
//                
//                for (int i = 0; i < rectKPTArray.size(); ++i)
//                {
//                    float response = rectKPTArray[i].response;
//                    
//                    int ir = 0;
//                    for ( list<pair<int,int> >::iterator iter = _targetKeypontsDistributionRange.begin(); iter != _targetKeypontsDistributionRange.end();++iter)
//                    {
//                        pair<int,int> range = *iter;
//                        
//                        if (response > range.first && response <= range.second)
//                        {
//                            respRangeKeypointsArray[ir].push_back(rectKPTArray[i]);
//                            break;
//                        }
//                        
//                        ++ir;
//                    }
//                }
//                
//                //先对和跟踪框的特征最不相似
//                int bestCount = (c_rectWH*c_rectWH)*dstKPTRegionalDensity;
//                
//                
//                for (int ik = respRangeKeypointsArray.size() - 1; ik >= 0; --ik)
//                {
//                    int respRangeKPTCount = respRangeKeypointsArray[ik].size();
//                    int willCount = newRectKPTArray.size() + respRangeKPTCount;
//                    if (willCount >  bestCount)
//                    {
//                        //全插入就多了，看看需要插入多少，就插入多少个
//                        int needAddCount = bestCount - newRectKPTArray.size();
//                        //小于最佳密度了，需要加回来
//                        //newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].begin() + needAddCount);
//                        
//                        for (float ii = 0; ii < respRangeKPTCount; ii = ii + (float)respRangeKPTCount/needAddCount)
//                        {
//                            newRectKPTArray.push_back(respRangeKeypointsArray[ik][(int)ii]);
//                        }
//                        
//                        break;
//                        
//                    }
//                    else if (willCount <  bestCount)
//                    {
//                        //把这个范围的全插入了，还小于最佳值，继续走循环看看下一个响应范围的吧
//                        newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].end());
//                    }
//                    else if (willCount ==  bestCount)
//                    {
//                        //so good
//                        newRectKPTArray.insert(newRectKPTArray.end(), respRangeKeypointsArray[ik].begin(),respRangeKeypointsArray[ik].end());
//                        break;
//                        
//                    }
//                    
//                }
//                
//            }
//            else
//            {
//                //理论上不会进这里
//                newRectKPTArray = rectKPTArray;
//            }
//            
//            //
//            delKPTCount = delKPTCount + rectKPTArray.size() - newRectKPTArray.size();
//            dstKeypoints.insert(dstKeypoints.end(), newRectKPTArray.begin(),newRectKPTArray.end());
//            
//        }
//        
//        //区域均值化后，特征点数量还是多，就只能随机抽样的
//        
//        if (TIMEDEBUG)
//        {
//            printf("CMTTIME 区域密度响应分布过滤法耗时:%.3f 过滤：%d 过滤前：%lu 过滤后:%lu  \n",(clock()-startCTime_all)*1000.0/CLOCKS_PER_SEC,delKPTCount,srcKeypoints.size(),dstKeypoints.size());
//        }
//        
//        
//    }
    
    
    void PerformanceAdapter::keypointsFilterByRDTD(const vector<KeyPoint> &srcKeypoints,vector<KeyPoint> &dstKeypoints,const Mat &reSizeGrayImage)
    {
        keypointsFilterByRegionalDensityThresholdDistribution(srcKeypoints,dstKeypoints,reSizeGrayImage, _initScale,_runingKPTRegionalDensity);
    }
    
    
    void PerformanceAdapter::calcRuningKeypoints(const Mat &im_RGBA,Mat &reSizeGrayImage, vector<KeyPoint> &keypoints)
    {
        
        //缩放图片
        if (_initScale < 1.0f)
        {
            clock_t startCTime = clock();
            
            
            cv::Size reSize = cv::Size(im_RGBA.cols*_initScale,im_RGBA.rows*_initScale);
            Mat reSizeImage_RGBA = cv::Mat(reSize,CV_8UC4);
            cv::resize(im_RGBA, reSizeImage_RGBA,reSize);
            
            cvtColor(reSizeImage_RGBA,reSizeGrayImage,CV_RGB2GRAY);
            
            printf("CMTTIME cmtTracking cvtColor reSize:%.3f\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
            
            MG_FASTX(reSizeGrayImage, keypoints, _runingFastThreshold);
        }
        else
        {
            clock_t startCTime = clock();
            
            cvtColor(im_RGBA,reSizeGrayImage,CV_RGB2GRAY);
            
            printf("CMTTIME cmtTracking cvtColor:%.3f\n",(clock()-startCTime)*1000.0/CLOCKS_PER_SEC);
            
            MG_FASTX(reSizeGrayImage, keypoints, _runingFastThreshold);
            
        }
        
        
    }
    
#define MGT_KPT_RD_PRE100X100(d) ((d)/(100.0*100.0))
    
    void PerformanceAdapter::MG_FASTX(const Mat &im_gray, vector<KeyPoint>& keypoints,int threshold)
    {
#if USER_FASTX
#if CV_MAJOR_VERSION > 2
        FAST(im_gray, keypoints, threshold,true,2 /*FeatureDetector::TYPE_9_16*/);
#else
        FASTX(im_gray, keypoints, threshold,true,2 /*FeatureDetector::TYPE_9_16*/);
#endif
#else
        
        detector->detect(im_gray, keypoints); // 检测初始图像的所有关键点
        
#endif
    }
    
    void PerformanceAdapter::calcBestThr(float matchPrecent,float frameRate,int targetCount,int newFrameTCount)
    {
        _curFrameRate = frameRate;
        //return;
        //debug 手动模式，直接退出
        if (_isManual)
        {
            return;
        }
        
        //帧率
        
        
        //如果识别率大于50%
        if (matchPrecent > 0.70)
        {
            if (frameRate > 30)
            {
                
            }
            else
            {
                //尝试增加帧率
                //减少特征点，减小密度阈值
                _runingKPTRegionalDensity -= MGT_KPT_RD_PRE100X100(10.0);
                
                if (TIMEDEBUG) {
                    printf("EVENT calcBestThr:_runingKPTRegionalDensity -= 10 = %d; matchPrecent:%.3f frameRate:%.2f\n",_runingKPTRegionalDensity*100*100,matchPrecent,frameRate);
                }
            }
        }
        else  if (matchPrecent > 0.30)
        {
            if (frameRate > 25)
            {
                //尝试增加识别可信度
                //增多特征点，增大密度
                _runingKPTRegionalDensity += MGT_KPT_RD_PRE100X100(10);
                
                if (TIMEDEBUG) {
                    printf("EVENT calcBestThr:_runingKPTRegionalDensity += 10 = %d; matchPrecent:%.3f frameRate:%.2f\n",_runingKPTRegionalDensity*100*100,matchPrecent,frameRate);
                }
            }
            else
            {
                //尝试增加帧率
                //减少特征点，减小密度阈值
                _runingKPTRegionalDensity -= MGT_KPT_RD_PRE100X100(10);
                
                if (TIMEDEBUG) {
                    printf("EVENT calcBestThr:_runingKPTRegionalDensity += 10 = %d; matchPrecent:%.3f frameRate:%.2f\n",_runingKPTRegionalDensity*100*100,matchPrecent,frameRate);
                }
            }
        }
        else  if (matchPrecent > 0.10)
        {
            if (frameRate > 20)
            {
                //尝试增加识别可信度
                //增多特征点，增大密度
                _runingKPTRegionalDensity += MGT_KPT_RD_PRE100X100(10);
                
                if (TIMEDEBUG) {
                    printf("EVENT calcBestThr:_runingKPTRegionalDensity += 10 = %d; matchPrecent:%.3f frameRate:%.2f\n",_runingKPTRegionalDensity*100*100,matchPrecent,frameRate);
                }
            }
            else
            {
                //尝试增加帧率
                
            }
        }
        else
        {
            //完全丢失目标
            //保持最小5帧，否则很难恢复识别
            
            if (frameRate > 5)
            {
                //尝试增加识别可信度
                //增多特征点，增大密度
                _runingKPTRegionalDensity += MGT_KPT_RD_PRE100X100(10);
                
                if (_runingKPTRegionalDensity < MGT_KPT_RD_PRE100X100(50))
                {
                    _runingKPTRegionalDensity = MGT_KPT_RD_PRE100X100(50);
                }
                
                
                
                if (TIMEDEBUG) {
                    printf("EVENT calcBestThr:_runingKPTRegionalDensity += 10 = %d; matchPrecent:%.3f frameRate:%.2f\n",_runingKPTRegionalDensity*100*100,matchPrecent,frameRate);
                }
            }
            else
            {
                //已经小于5帧了，还是没有找到呢
            }
            
        }
        
        
        
        _runingKPTRegionalDensity =  _runingKPTRegionalDensity < MGT_TARGET_MIN_KP_PERCENT?MGT_TARGET_MIN_KP_PERCENT:_runingKPTRegionalDensity;
        _runingKPTRegionalDensity =  _runingKPTRegionalDensity > MGT_TARGET_BEST_KP_PERCENT?MGT_TARGET_BEST_KP_PERCENT:_runingKPTRegionalDensity;
        
        _runingFastThreshold =  _runingFastThreshold < MGT_IMAGE_FAST_THR_MIN?MGT_IMAGE_FAST_THR_MIN:_runingFastThreshold;
        _runingFastThreshold =  _runingFastThreshold > MGT_IMAGE_FAST_THR_MAX?MGT_IMAGE_FAST_THR_MAX:_runingFastThreshold;
        
        _initScale = _initScale < MGT_IMAGE_MIN_SCALE?MGT_IMAGE_MIN_SCALE:_initScale;
        _initScale = _initScale > 1.0?1.0:_initScale;
    }
    
    
    bool PerformanceAdapter::canPerdictorTrack()
    {
        if (_perTrackCount < 0)
        {
            ++_perTrackCount;
            return false;
        }
        else if (_perTrackCount > _curFrameRate/2)
        {
            //每秒查两次全检测帧，每次连续插两个
            _perTrackCount = -2;
            return false;
        }
        
        return true;
        
    }
    
    void PerformanceAdapter::perdictorTrackCount()
    {
        ++_perTrackCount;
    }
    
    
} /* namespace pa */
