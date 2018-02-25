#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include "CMT/MGCMT.h"
#include "LibCMT.h"

#define  LIBCMT  true

using namespace std;
using namespace cv;
using namespace cmt;

extern "C" {

typedef enum {
    NV21 = 0,
    RGB,
    RGBA
} DataType;

bool CMTinitiated = false;
CMT *libCMT = new CMT();
// LibCMT * g_cmt = new LibCMT();
int CmtWidth = 400;
int CmtHeight = 300;

uint8_t *g_dataBuff = NULL;
int g_dataSize = 0;
char *g_debufInfo = NULL;

void allocateDataBuff(int size) {
    if (g_dataSize >= size) {
        return;
    }
    if (g_dataBuff != NULL) {
        delete[] g_dataBuff;
        g_dataBuff = NULL;
    }
    g_dataBuff = new uint8_t[size];
    g_dataSize = size;
}

// JNIEXPORT jbyteArray JNICALL Java_com_melonteam_objtrack_ObjTrack_getYUVData(JNIEnv *env, jobject,jintArray size){
//         jbyteArray jbArray = env->NewByteArray(g_FrameSize);
//         pthread_mutex_lock(&mutex);
//         env->SetByteArrayRegion(jbArray, 0, g_FrameSize, (jbyte*)g_Frame);
//         pthread_mutex_unlock(&mutex);
//
//         env->SetIntArrayRegion(size,0,2,g_Size);
//         return jbArray;
// }

Mat getTrackMat(uint8_t *buf, int dataType, int width, int height) {
    Mat resultMat, img, tmp, grayMat;
    switch (dataType) {
        case NV21:
            tmp = Mat(height + height / 2, width, CV_8UC1, buf);
            cv::cvtColor(tmp, img, CV_YUV420sp2GRAY);
            if (CmtWidth == width && CmtHeight == height) {
#ifdef LIBCMT
                cv::cvtColor(img, resultMat, CV_GRAY2RGBA);
#else
                resultMat = img;
#endif
            } else {
#ifdef LIBCMT
                cv::resize(img, grayMat, cv::Size(CmtWidth, CmtHeight));
                cv::cvtColor(grayMat, resultMat, CV_GRAY2RGBA);
#else
                cv::resize(img,resultMat,cv::Size(CmtWidth,CmtHeight));
#endif
            }
            break;
        case RGB:
            tmp = Mat(height, width, CV_8UC3, buf);
            if (CmtWidth == width && CmtHeight == height) {
#ifdef LIBCMT
                cv::cvtColor(tmp, resultMat, CV_RGB2RGBA);
#else
                cv::cvtColor(tmp,resultMat,CV_RGB2GRAY);
#endif
            } else {
#ifdef LIBCMT
                cv::resize(tmp, img, cv::Size(CmtWidth, CmtHeight));
                cv::cvtColor(img, resultMat, CV_RGB2RGBA);
#else
                cv::resize(tmp,img,cv::Size(CmtWidth,CmtHeight));
                cv::cvtColor(img,resultMat,CV_RGB2GRAY);
#endif
            }
            break;
        case RGBA:
            if (CmtWidth == width && CmtHeight == height) {
#ifdef LIBCMT
                resultMat = Mat(height, width, CV_8UC4, buf);
#else
                img = Mat(height,width,CV_8UC4,buf);
                cv::cvtColor(img,resultMat,CV_RGBA2GRAY);
#endif
            } else {
                tmp = Mat(height, width, CV_8UC4, buf);
#ifdef LIBCMT
                cv::resize(tmp, resultMat, cv::Size(CmtWidth, CmtHeight));
#else
                cv::resize(tmp,img,cv::Size(CmtWidth,CmtHeight));
                cv::resize(img,resultMat,cv::Size(CmtWidth,CmtHeight));
#endif
            }
            break;
    }
    return resultMat;
}

void caluateResizeSize(int width, int height) {
    float rateX = 1.0f;
    float rateY = 1.0f;
    CmtWidth = width;
    CmtHeight = height;
    if (width >= 600 || height >= 600) {
        rateX = 400 * 1.0f / width;
        rateY = 400 * 1.0f / height;
        if (rateX < rateY) {
            CmtWidth = 400;
            CmtHeight = (int) (height * rateX);
            rateY = rateX;
        } else {
            CmtHeight = 400;
            CmtWidth = (int) (width * rateY);
            rateX = rateY;
        }
    }
}

JNIEXPORT jstring JNICALL
Java_com_melonteam_mgtracker_MGTracker_getDebugJson(JNIEnv *env, jobject) {
    if (!CMTinitiated) {
        return NULL;
    }
    if (g_debufInfo == NULL) {
        g_debufInfo = new char[150];
    }
    sprintf(g_debufInfo,
            "{\"trackFrame\":%.2f,\"trackCost\":%.2f,\"trackScale\":%.2f,\"trackDensity\":%ld,\"matchPercent\":%.1f,\"isMatch\":%d,\"activePoints\":%d,\"targetPoints\":%d,\"framePoints\":%d,\"predictPoints\":%d,\"rdtdCount\":%d}",
            libCMT->track30AvgFrameRate, libCMT->trackCurFrameCost, libCMT->_perfAdapter._initScale,
            static_cast<long>(libCMT->_perfAdapter._runingKPTRegionalDensity * 100 * 100),
            libCMT->_matchedPrecent, libCMT->_isCurProdictorMatched,
            libCMT->_activePoints.size(), libCMT->_matcher.target_length,
            libCMT->_curFrameKeypointsCount, libCMT->_curPreditorFilterFrameKeypointsCount,
            libCMT->_curFilterByRDTDKPTCount
    );
    return env->NewStringUTF(reinterpret_cast<const char *>(g_debufInfo));
}

JNIEXPORT jboolean JNICALL
Java_com_melonteam_mgtracker_MGTracker_openTrack(JNIEnv *env, jobject, jbyteArray yuvData,
                                                 jint dataType, jint x, jint y, jint width,
                                                 jint height, jint imageWidth, jint imageHeight) {
    int len = env->GetArrayLength(yuvData);
    jboolean initBoolean = JNI_FALSE;

    //ALOG("x:%d,y:%d,w:%d,h:%d,width:%d,height:%d,len:%d",x,y,width,height,imageWidth,imageHeight,len);
    allocateDataBuff(len);
    caluateResizeSize(imageWidth, imageHeight);
    env->GetByteArrayRegion(yuvData, 0, len, reinterpret_cast<jbyte *>(g_dataBuff));
    float rateX = CmtWidth * 1.0f / imageWidth;
    float rateY = CmtHeight * 1.0f / imageHeight;
    Point p1(x * rateX, y * rateY);
    Point p2((x + width) * rateX, (y + height) * rateY);
    Rect rect = Rect(p1, p2);
    Mat resultMat = getTrackMat(g_dataBuff, dataType, imageWidth, imageHeight);
#ifdef LIBCMT
    CMTinitiated = libCMT->initialize(resultMat, rect);
    initBoolean = JNI_TRUE;
#else
    CMTinitiated = g_cmt->initialise(resultMat,p1,p2);
#endif
    return initBoolean;
}

JNIEXPORT jintArray JNICALL
Java_com_melonteam_mgtracker_MGTracker_getPredictTrack(JNIEnv *env, jobject, jint imageWidth,
                                                       jint imageHeight) {
    if (!CMTinitiated)
        return NULL;
    Rect rect = libCMT->_predictorRect;
    jint fill[8];
    jintArray result = env->NewIntArray(8);
    float rateX = imageWidth * 1.0f / CmtWidth;
    float rateY = imageHeight * 1.0f / CmtHeight;
    int width = rect.width * rateX;
    int height = rect.height * rateY;
    fill[0] = (int) rect.x * rateX;
    fill[1] = (int) rect.y * rateY;
    fill[2] = (int) rect.x * rateX + width;
    fill[3] = (int) rect.y * rateY;
    fill[4] = (int) rect.x * rateX;
    fill[5] = (int) rect.y * rateY + height;
    fill[6] = (int) rect.x * rateX + width;
    fill[7] = (int) rect.y * rateY + height;
    env->SetIntArrayRegion(result, 0, 8, fill);
    return result;
}

JNIEXPORT jintArray JNICALL
Java_com_melonteam_mgtracker_MGTracker_processTrack(JNIEnv *env, jobject, jbyteArray yuvData,
                                                    jint dataType, jint imageWidth,
                                                    jint imageHeight) {
    if (!CMTinitiated)
        return NULL;
    int len = env->GetArrayLength(yuvData);
    allocateDataBuff(len);
    jintArray result = env->NewIntArray(8);
    Rect rect;
    env->GetByteArrayRegion(yuvData, 0, len, reinterpret_cast<jbyte *>(g_dataBuff));
    Mat resultMat = getTrackMat(g_dataBuff, dataType, imageWidth, imageHeight);
#ifdef LIBCMT
    RotatedRect resultRect;
    libCMT->processFrame(resultMat);
    if (libCMT->_isMatched) {
        resultRect = libCMT->_outRot;
    } else {
        resultRect = RotatedRect(Point2f(0, 0), Size2f(0, 0), 0);
    }
    rect.x = resultRect.center.x - resultRect.size.width / 2;
    rect.y = resultRect.center.y - resultRect.size.height / 2;
    rect.width = resultRect.size.width;
    rect.height = resultRect.size.height;
#else
    g_cmt->processFrame(resultMat);
    rect.x = g_cmt->topLeft.x;
    rect.y = g_cmt->topLeft.y;
    rect.width = g_cmt->bottomRight.x-g_cmt->topLeft.x;
    rect.height = g_cmt->bottomRight.y-g_cmt->topLeft.y;
#endif

    jint fill[8];

    float rateX = imageWidth * 1.0f / CmtWidth;
    float rateY = imageHeight * 1.0f / CmtHeight;
    int width = rect.width * rateX;
    int height = rect.height * rateY;
    fill[0] = (int) rect.x * rateX;
    fill[1] = (int) rect.y * rateY;
    fill[2] = (int) rect.x * rateX + width;
    fill[3] = (int) rect.y * rateY;
    fill[4] = (int) rect.x * rateX;
    fill[5] = (int) rect.y * rateY + height;
    fill[6] = (int) rect.x * rateX + width;
    fill[7] = (int) rect.y * rateY + height;
    env->SetIntArrayRegion(result, 0, 8, fill);
    return result;
}

}
