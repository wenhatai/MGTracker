#include "Matcher.h"

using cv::vconcat;
using cv::DMatch;

namespace cmt {
    
    void Matcher::initialize(const vector<Point2f> & pts_fg_norm, const Mat desc_fg, const vector<int> & classes_fg,
                             const Mat desc_bg, const Point2f center)
    {
        
        //Copy normalized points 存储 正规化的点
        this->pts_fg_norm = pts_fg_norm;
        
        //Remember number of background points 存储背景的特征点的数量
        this->num_bg_points = desc_bg.rows;
        
        //Form database by stacking background and foreground features
        // 合成前景和背景的特征到一个Mat文件中
        if (desc_bg.rows > 0 && desc_fg.rows > 0)
            vconcat(desc_bg, desc_fg, database);
        else if (desc_bg.rows > 0)
            database = desc_bg;
        else
            database = desc_fg;
        
        //Extract descriptor length from features 根据特征抽取描述其长度
        desc_length = database.cols*8;
        
        //Create background classes (-1) 创建背景类
        vector<int> classes_bg = vector<int>(desc_bg.rows,-1);
        
        //Concatenate fg and bg classes 连接前景和背景的类
        classes = classes_bg;
        classes.insert(classes.end(), classes_fg.begin(), classes_fg.end());
        
        //Create descriptor matcher 创建描述匹配器BruteForce-Hamming
        bfmatcher = DescriptorMatcher::create(MGT_DescriptorMatcherType);
        
        target_length = pts_fg_norm.size();
        matcher_length = target_length + desc_bg.rows;
        
        if (target_length == 0 || database.cols == 0)
        {
              printf("Matcher::initialize err target_length:%d database:%d\n",target_length,database.cols);
        }
        
    }
    
    void Matcher::matchGlobal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                              vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched)
    {
        //FILE_LOG(logDEBUG) << "Matcher::matchGlobal() call";
        
        if (keypoints.size() == 0)
        {
            //FILE_LOG(logDEBUG) << "Matcher::matchGlobal() return";
            return;
        }
        
        vector<vector<DMatch> > matches;
        bfmatcher->knnMatch(descriptors, database, matches, 2);

        
        for (size_t i = 0; i < matches.size(); i++)
        {
            vector<DMatch> m = matches[i];
            
            float distance1 = m[0].distance / desc_length;
            float distance2 = m[1].distance / desc_length;
            int matched_class = classes[m[0].trainIdx];
            
            if (matched_class == -1) continue;
            if (distance1 > thr_dist) continue;
            if (distance1/distance2 > thr_ratio) continue;
            
            points_matched.push_back(keypoints[i].pt);
            classes_matched.push_back(matched_class);
            
            //当前帧关键点被匹配上的索引
            curkpts_classes_matched.push_back(i);
        }
        
        //
        
        
        //FILE_LOG(logDEBUG) << "Matcher::matchGlobal() return";
    }
    
    void Matcher::matchLocal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                             const Point2f center, const float scale, const float rotation,
                             vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched)
    {
        
        if (keypoints.size() == 0) {
            return;
        }
        
        //Transform initial points
        vector<Point2f> pts_fg_trans;
        pts_fg_trans.reserve(pts_fg_norm.size());
        for (size_t i = 0; i < pts_fg_norm.size(); i++)
        {
            // 同样是计算相对位置
            pts_fg_trans.push_back(scale * rotate(pts_fg_norm[i], -rotation));
        }
        float thr_cutoff2 = thr_cutoff*thr_cutoff;
        
        
        //xperf
        double startCTime , startCTime2;
        startCTime = now_ms();
        startCTime2 = now_ms();
        
        double sSumtime = 0;
        
        
        //Perform local matching
        for (size_t i = 0; i < keypoints.size(); i++)
        {
            //Normalize keypoint with respect to center
            Point2f location_rel = keypoints[i].pt - center;
            
            double startCTime_S = now_ms();
            
            //Find potential indices for matching
            vector<int> indices_potential;
            for (size_t j = 0; j < pts_fg_trans.size(); j++)
            {
                if (1) {
                    // 计算位置偏差
                    float diffX = pts_fg_trans[j].x - location_rel.x;
                    float diffY = pts_fg_trans[j].y - location_rel.y;
                    float l2norm = diffX*diffX + diffY*diffY;
                    
                    // 设置一个阈值
                    if (l2norm < thr_cutoff2) {
                        indices_potential.push_back(num_bg_points + j);
                    }
                } else {
                    // 计算位置偏差
                    float l2norm = norm(pts_fg_trans[j] - location_rel);
                    
                    // 设置一个阈值
                    if (l2norm < thr_cutoff) {
                        indices_potential.push_back(num_bg_points + j);
                    }
                }
                
                
                
            }
            
            sSumtime += (now_ms() - startCTime_S);
            
            //If there are no potential matches, continue
            if (indices_potential.size() == 0) continue;
            
            //Build descriptor matrix and classes from potential indices
            Mat database_potential = Mat(indices_potential.size(), database.cols, database.type());
            for (size_t j = 0; j < indices_potential.size(); j++) {
                database.row(indices_potential[j]).copyTo(database_potential.row(j));
            }
            
            //Find distances between descriptors
            vector<vector<DMatch> > matches;
            // 对选出的特征点进行特征匹配
            bfmatcher->knnMatch(descriptors.row(i), database_potential, matches, 2);
            
            vector<DMatch> m = matches[0];
            
            float distance1 = m[0].distance / desc_length;
            float distance2 = m.size() > 1 ? m[1].distance / desc_length : 1;
            
            if (distance1 > thr_dist) continue;
            if (distance1/distance2 > thr_ratio) continue;
            
            int matched_class = classes[indices_potential[m[0].trainIdx]];
            
            points_matched.push_back(keypoints[i].pt);
            classes_matched.push_back(matched_class);
            
            //当前帧关键点被匹配上的索引
            curkpts_classes_matched.push_back(i);
        }
        
        
        if (TIMEDEBUG) {
            double allT = (now_ms()-startCTime);
            printf("CMTTIME matchLocal all:%.3f,find:%.3f\n",allT*1000.0/CLOCKS_PER_SEC, sSumtime*1000.0/CLOCKS_PER_SEC);
        }
        
    }
    
    
    
    
    /////////////////========
    ModelLearnMatcher::ModelLearnMatcher()
    {

         bfmatcher = DescriptorMatcher::create("BruteForce-Hamming");
        firstKeypontsCount = 0;
        useLearnMatcher = false;
    }
    
    void ModelLearnMatcher::initialize(const vector<KeyPoint> & keypoints, const Mat &desc_fg,const vector<Point2f> & pts_fg_norm,cv::Size size,const Point2f center)
    {
        if (!useLearnMatcher)
        {
            return;
        }
        
        kptArray.clear();

        for (int i = 0; i < keypoints.size(); ++i)
        {
            LearnKeypointModel m;
            m.keyPoint = keypoints[i];
            m.pt = m.keyPoint.pt;
            m.appearCount = 1;
            
            kptArray.push_back(m);
        }
        
        learnDatabase = desc_fg;

        firstKeypontsCount = kptArray.size();
        
        TargetModel tm;
        tm.center = center;
        tm.size = size;
        tm.pts_fg_norm = pts_fg_norm;
        
        desc_length = learnDatabase.cols*8;

    }
    
    void ModelLearnMatcher::updateModel(const vector<KeyPoint> &keypoints,const vector<int> &curTargetMatchedkeypoints,const vector<int> &curTargetMatchedkeypointsIndex,const vector<int> &curTargetNewkeypoints, const Mat &descriptors,cv::Size size,const Point2f center)
    {
        if (!useLearnMatcher)
        {
            return;
        }
        
        double startCTime_all = now_ms();
        //
        TargetModel tm;
        tm.center = center;
        tm.size = size;
        
        for (size_t i = 0; i < curTargetMatchedkeypoints.size(); i++)
        {
            int index = curTargetMatchedkeypoints[i];
            int kptArrayIndex =curTargetMatchedkeypointsIndex[i];
            
            tm.kptArrayIndex2NormPT[kptArrayIndex] = keypoints[index].pt - center;
        }
        
        
        //Build descriptor matrix and classes from potential indices
        Mat new_kpt_des = Mat(curTargetNewkeypoints.size(), descriptors.cols, descriptors.type());
        
        for (size_t j = 0; j < curTargetNewkeypoints.size(); j++)
        {
            int index = curTargetNewkeypoints[j];
            descriptors.row(index).copyTo(new_kpt_des.row(j));
        }
        
        
        vector<LearnKeypointModel>  noMKeypoints;
        Mat descNoM = Mat(0, descriptors.cols, descriptors.type());
        set<int> matchedIndexSet;
        
        if (new_kpt_des.size > 0 && learnDatabase.rows > firstKeypontsCount)
        {
            //Find distances between descriptors
            vector<vector<DMatch> > matches;
            // 对选出的特征点进行特征匹配
            bfmatcher->knnMatch(new_kpt_des, learnDatabase.rowRange(firstKeypontsCount, learnDatabase.rows ), matches, 2);
            
            for (size_t i = 0; i < matches.size(); i++)
            {
                int kptIndex = curTargetNewkeypoints[i];
                KeyPoint kpt =  keypoints[kptIndex];
                
                
                vector<DMatch> m = matches[i];
                
                float distance1 = m[0].distance / desc_length;
                float distance2 = m[1].distance / desc_length;
                
                if ((distance1 > mgt_thr_dist || distance1/distance2 > mgt_thr_ratio) )
                {
                    LearnKeypointModel model;
                    model.keyPoint = kpt;
                    model.pt = model.keyPoint.pt;
                    model.appearCount = 1;
                    noMKeypoints.push_back(model);
                    descNoM.push_back(new_kpt_des.row(i));
                    
                }
                else
                {
                    //匹配到了，更新信息
                    int mIndex = m[0].trainIdx + firstKeypontsCount;
                    matchedIndexSet.insert(mIndex);
                    LearnKeypointModel &m = kptArray.at(mIndex);
                    
                    m.keyPoint = kpt;
                    m.pt = m.keyPoint.pt;
                    
                    m.appearCount += 1;
                    m.infoUpdateFlag = true;
                    m.descUpdateFlag = true;
                    m.continueDisappearCount = 0;
                    
                    tm.kptArrayIndex2NormPT[mIndex] = m.pt - center;
                    
                    
                    new_kpt_des.row(i).copyTo(learnDatabase.row(mIndex));
                    
                    if (mIndex < firstKeypontsCount)
                    {
                        printf("EVENT updateModel: ERR 新加入的特征点匹配到了原始特征 mIndex < firstKeypontsCount\n");
                    }
                }
                
            }
            
        }
        else if (new_kpt_des.size > 0)
        {
            for (int i= 0; i < curTargetNewkeypoints.size(); ++i)
            {
                int kptIndex = curTargetNewkeypoints[i];
                KeyPoint kpt =  keypoints[kptIndex];
                
                LearnKeypointModel model;
                model.keyPoint = kpt;
                model.pt = model.keyPoint.pt;
                model.appearCount = 1;
                noMKeypoints.push_back(model);
                
            }
            
            descNoM = new_kpt_des;

        }
        

        int canDelCount = 0;
        int canUse = 0;
        
        for (int i = firstKeypontsCount; i < kptArray.size(); ++i)
        {
            LearnKeypointModel &m = kptArray.at(i);
            
            if (matchedIndexSet.count(i) <= 0)
            {
                m.disappearCount += 1;
                m.continueDisappearCount +=1;
            }
            
            if (m.canDel())
            {
                ++canDelCount;
            }
            
            if (m.canUse() && tm.kptArrayIndex2NormPT.count(i) > 0)
            {
                ++canUse;
            }
        }
        
        //30%可以删除时，触发一次垃圾回收
        if ( ((float)canDelCount/(kptArray.size() - firstKeypontsCount)) > 0.2)
        {
            long needSize = firstKeypontsCount + kptArray.size() - canDelCount;
            vector<LearnKeypointModel> newKptArray(kptArray.begin(), kptArray.begin() + firstKeypontsCount);

            Mat newLearnDatabase = Mat(learnDatabase.rowRange(0, firstKeypontsCount));
        
            for (int i = firstKeypontsCount; i < kptArray.size(); ++i)
            {
                LearnKeypointModel &m = kptArray.at(i);
                
                if (!m.canDel())
                {
                    newKptArray.push_back(m);
                    newLearnDatabase.push_back(learnDatabase.row(i));
                    
                    if ( tm.kptArrayIndex2NormPT.count(i) > 0)
                    {
                        tm.kptArrayIndex2NormPT.erase(i);
                    }
                }
                else
                {
                    if (m.canUse() ) {
                        printf("EVENT updateModel ....... \n");
                        m.printLog();
                    }
                }
            }
            
            kptArray = newKptArray;
            learnDatabase = newLearnDatabase;

        }
        
       
        //插入全新的特征点
        kptArray.insert(kptArray.end(), noMKeypoints.begin(), noMKeypoints.end() );
        
        if (learnDatabase.rows > 0 && descNoM.rows > 0)
        {
            Mat newLearnDatabase = Mat(learnDatabase.rows + descNoM.rows, descriptors.cols, descriptors.type());
            vconcat(learnDatabase, descNoM, newLearnDatabase);
            learnDatabase = newLearnDatabase;
        }
 
        

        modelList.push_back(tm);
        
        while (modelList.size() > 3)
        {
            modelList.pop_front();
        }
        
        
        if (TIMEDEBUG) {
            printf("CMTTIME updateModel:%.3f firstKeypontsCount:%d kptArray:%lu canUse:%d canDel:%d\n",(now_ms()- startCTime_all)*1000.0/CLOCKS_PER_SEC,firstKeypontsCount,kptArray.size() - firstKeypontsCount,canUse,canDelCount);
            
            printf("EVENT updateModel 以匹配首帧的特征点：%d 匹配sst数：%d 全新：%d \n",curTargetMatchedkeypoints.size()  ,curTargetNewkeypoints.size() - noMKeypoints.size(),noMKeypoints.size());
        }
        
    }
    
    
    
    void ModelLearnMatcher::matchGlobal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                                        vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched)
    {
        //FILE_LOG(logDEBUG) << "Matcher::matchGlobal() call";
        
        if (keypoints.size() == 0)
        {
            //FILE_LOG(logDEBUG) << "Matcher::matchGlobal() return";
            return;
        }
        
    
        
        TargetModel tm = modelList.back();
        curLearnDatabase =  Mat(0, descriptors.cols, descriptors.type());
        curLearnDatabase.reserve(200);

        
        curKptArrayIndex.clear();
        
        for (int i = 0; i < kptArray.size(); ++i)
        {
            LearnKeypointModel &m = kptArray.at(i);

            
            if (m.canUse() && tm.kptArrayIndex2NormPT.count(i) > 0)
            {
                curKptArrayIndex.push_back(i);
                curLearnDatabase.push_back(learnDatabase.row(i));
            }
        }
        
        vector<vector<DMatch> > matches;
        bfmatcher->knnMatch(descriptors, curLearnDatabase, matches, 2);
        
        for (size_t i = 0; i < matches.size(); i++)
        {
            vector<DMatch> m = matches[i];
            
            float distance1 = m[0].distance / desc_length;
            float distance2 = m[1].distance / desc_length;
           
        
            if (distance1 > mgt_thr_dist) continue;
            if (distance1/distance2 > mgt_thr_ratio) continue;
            
            int matched_class = curKptArrayIndex[m[0].trainIdx];
            
            points_matched.push_back(tm.kptArrayIndex2NormPT[matched_class]);
            classes_matched.push_back(matched_class);
            
            curkpts_classes_matched.push_back(i);
        }
        
        //FILE_LOG(logDEBUG) << "Matcher::matchGlobal() return";
    }
    

    void ModelLearnMatcher::matchLocal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                             const Point2f center, const float scale, const float rotation,
                             vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched)
    {
//        
//        if (keypoints.size() == 0) {
//            return;
//        }
//        
//        TargetModel tm = modelList.back();
//        
//        //Transform initial points
//        vector<Point2f> pts_fg_trans;
//        pts_fg_trans.reserve(tm.pts_fg_norm.size());
//        for (size_t i = 0; i < tm.pts_fg_norm.size(); i++)
//        {
//            // 同样是计算相对位置
//            pts_fg_trans.push_back(scale * rotate(tm.pts_fg_norm[i], -rotation));
//        }
//        float thr_cutoff2 = mgt_thr_cutoff*mgt_thr_cutoff;
//        
//        
//        //xperf
//        double startCTime , startCTime2;
//        startCTime = now_ms();
//        startCTime2 = now_ms();
//        
//        double sSumtime = 0;
//        
//        
//        //Perform local matching
//        for (size_t i = 0; i < keypoints.size(); i++)
//        {
//            //Normalize keypoint with respect to center
//            Point2f location_rel = keypoints[i].pt - center;
//            
//            double startCTime_S = now_ms();
//            
//            //Find potential indices for matching
//            vector<int> indices_potential;
//            for (size_t j = 0; j < pts_fg_trans.size(); j++)
//            {
//                if (1) {
//                    // 计算位置偏差
//                    float diffX = pts_fg_trans[j].x - location_rel.x;
//                    float diffY = pts_fg_trans[j].y - location_rel.y;
//                    float l2norm = diffX*diffX + diffY*diffY;
//                    
//                    // 设置一个阈值
//                    if (l2norm < thr_cutoff2) {
//                        indices_potential.push_back(num_bg_points + j);
//                    }
//                } else {
//                    // 计算位置偏差
//                    float l2norm = norm(pts_fg_trans[j] - location_rel);
//                    
//                    // 设置一个阈值
//                    if (l2norm < thr_cutoff) {
//                        indices_potential.push_back(num_bg_points + j);
//                    }
//                }
//                
//                
//                
//            }
//            
//            sSumtime += (now_ms() - startCTime_S);
//            
//            //If there are no potential matches, continue
//            if (indices_potential.size() == 0) continue;
//            
//            //Build descriptor matrix and classes from potential indices
//            Mat database_potential = Mat(indices_potential.size(), database.cols, database.type());
//            for (size_t j = 0; j < indices_potential.size(); j++) {
//                database.row(indices_potential[j]).copyTo(database_potential.row(j));
//            }
//            
//            //Find distances between descriptors
//            vector<vector<DMatch> > matches;
//            // 对选出的特征点进行特征匹配
//            bfmatcher->knnMatch(descriptors.row(i), database_potential, matches, 2);
//            
//            vector<DMatch> m = matches[0];
//            
//            float distance1 = m[0].distance / desc_length;
//            float distance2 = m.size() > 1 ? m[1].distance / desc_length : 1;
//            
//            if (distance1 > thr_dist) continue;
//            if (distance1/distance2 > thr_ratio) continue;
//            
//            int matched_class = classes[indices_potential[m[0].trainIdx]];
//            
//            points_matched.push_back(keypoints[i].pt);
//            classes_matched.push_back(matched_class);
//            
//            //当前帧关键点被匹配上的索引
//            curkpts_classes_matched.push_back(i);
//        }
//        
//        
//        if (TIMEDEBUG) {
//            double allT = (now_ms()-startCTime);
//            printf("CMTTIME matchLocal all:%.3f,find:%.3f\n",allT*1000.0/CLOCKS_PER_SEC, sSumtime*1000.0/CLOCKS_PER_SEC);
//        }
        
    }
    

    
    
} /* namespace CMT */
