#ifndef MATCHER_H

#define MATCHER_H

#include "common_cmt.h"

#include "opencv2/features2d/features2d.hpp"


#define mgt_thr_dist (0.25)
#define mgt_thr_ratio (0.8)
#define mgt_thr_cutoff (20)
using cv::KeyPoint;
using cv::DescriptorMatcher;

namespace cmt {

    class Matcher
    {
    public:
        Matcher() : thr_dist(0.25), thr_ratio(0.8), thr_cutoff(20) {};
        virtual ~Matcher(){};
        virtual void initialize(const vector<Point2f> & pts_fg_norm, const Mat desc_fg, const vector<int> & classes_fg,
                        const Mat desc_bg, const Point2f center);
        virtual void matchGlobal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                         vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched);
        virtual void matchLocal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                        const Point2f center, const float scale, const float rotation,
                        vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched);

        int target_length; //目标特征长度
        int matcher_length;//全部特征

    protected:
        vector<Point2f> pts_fg_norm;
        Mat database;
        vector<int> classes;
        int desc_length;
        int num_bg_points;
        cv::Ptr<DescriptorMatcher> bfmatcher;
        float thr_dist;
        float thr_ratio;
        float thr_cutoff;
    };



    class LearnKeypointModel
    {
    public:

        LearnKeypointModel() : index(0), appearCount(0), disappearCount(0),continueDisappearCount(0),descUpdateFlag(false),infoUpdateFlag(false),delFlag(false) {};
    public:

        bool canUse()
        {
            float WMAX = 0.4;
            return calcW() >= WMAX;
        }

        float calcW()
        {
            int y = 2;
            float w = (float)appearCount/(float)(appearCount + MAX( disappearCount,y));

            //ALOG("EVENT calcW: appearCount%d disappearCount%d appearCount/disappearCount:%.2f,ct:%d\n",appearCount,disappearCount,w,continueDisappearCount);
            return w;
        }

        bool canDel()
        {
            return continueDisappearCount > 20;
        }

        void printLog()
        {
            ALOG("EVENT calcW: appearCount%d disappearCount%d appearCount/disappearCount:%.2f,ct:%d\n",appearCount,disappearCount,calcW(),continueDisappearCount);
        }


    public:

        cv::KeyPoint keyPoint;
        cv::Point2f pt;
        int index;

        int appearCount;
        int disappearCount;

        int continueDisappearCount;

        bool descUpdateFlag;
        bool infoUpdateFlag;

        bool delFlag;
    };

    class TargetModel
    {
    public:
        TargetModel(): scale(1) {};
        vector<Point2f> pts_fg_norm;
        float scale;
        Point2f center;
        cv::Size size;

        map<int,Point2f> kptArrayIndex2NormPT;

    };

    class ModelLearnMatcher
    {
    public:
        ModelLearnMatcher()  ;

        virtual void initialize(const vector<KeyPoint> & keypoints, const Mat &desc_fg,const vector<Point2f> & pts_fg_norm,cv::Size size,const Point2f center);

        void updateModel(const vector<KeyPoint> &keypoints,const vector<int> &curTargetMatchedkeypoints,const vector<int> &curTargetMatchedkeypointsIndex,const vector<int> &curTargetNewkeypoints, const Mat &descriptors,cv::Size size,const Point2f center);

        void matchGlobal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                         vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched);

        void matchLocal(const vector<KeyPoint> & keypoints, const Mat descriptors,
                                           const Point2f center, const float scale, const float rotation,
                                           vector<Point2f> & points_matched, vector<int> & classes_matched,vector<int> & curkpts_classes_matched);

    private:


        int desc_length;
        vector<LearnKeypointModel> kptArray;
        Mat learnDatabase;
        int firstKeypontsCount;

        cv::Ptr<DescriptorMatcher> bfmatcher;

        list<TargetModel> modelList;
        TargetModel firstModel;

        Mat curLearnDatabase;
        vector<int> curKptArrayIndex;

            bool  useLearnMatcher;

    };

} /* namespace CMT */

#endif /* end of include guard: MATCHER_H */
