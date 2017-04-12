#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include "CMT/MGCMT.h"
#include "LibCMT.h"
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h> //usleep
#include <time.h>
#include <pthread.h>

#include <android/log.h>

#define  LOG_TAG    "testjni"
#define  LIBCMT  true
#define  ALOG(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

using namespace std;
using namespace cv;
using namespace cmt;

extern "C" {
  #include "libavcodec/avcodec.h"
  #include "libavformat/avformat.h"
  #include "libswscale/swscale.h"

bool CMTinitiated=false;
CMT * libCMT= new CMT();
// LibCMT * g_cmt = new LibCMT();
int CmtWidth = 400;
int CmtHeight = 300;

uint8_t *g_Frame = NULL;
int g_FrameSize;
pthread_mutex_t mutex;
jint *g_Size = new jint[2];
jbyteArray g_ByteArray = NULL;
jintArray g_SizeArray = NULL;
bool g_StopVideo = false;
bool g_PauseVideo = true;
Rect g_VideoRect;
bool g_ProcessEnd = false;
bool g_initTrackRect = false;


// from android samples
/* return current time in milliseconds */
static double now_ms(void) {
        struct timespec res;
        clock_gettime(CLOCK_MONOTONIC, &res);
        return 1000.0 * res.tv_sec + (double) res.tv_nsec / 1e6;
}

void allocateFrameSize(JNIEnv *env,int size){
        if(g_FrameSize >= size) {
                g_FrameSize = size;
                return;
        }
        if(g_Frame != NULL) {
                delete[] g_Frame;
                g_Frame = NULL;
        }
        g_Frame = new uint8_t[size];
        g_FrameSize = size;
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
JNIEXPORT jint JNICALL Java_com_melonteam_objtrack_ObjTrack_releaseVideo(JNIEnv *env,jobject obj){
        g_StopVideo = true;
}

JNIEXPORT jint JNICALL Java_com_melonteam_objtrack_ObjTrack_pauseVideo(JNIEnv *env,jobject obj){
        g_PauseVideo = true;
}

JNIEXPORT jint JNICALL Java_com_melonteam_objtrack_ObjTrack_playVideo(JNIEnv *env,jobject obj){
        g_PauseVideo = false;
}

//left,top,right,bottom
JNIEXPORT void JNICALL Java_com_melonteam_objtrack_ObjTrack_setVideoTrack(JNIEnv *env,jobject obj,jintArray location){
        jint* rect = env->GetIntArrayElements(location,NULL);
        Point p1(rect[0],rect[1]);
        Point p2(rect[2],rect[3]);
        pthread_mutex_lock(&mutex);
        g_VideoRect = Rect(p1,p2);
        pthread_mutex_unlock(&mutex);
        CMTinitiated = false;
        g_initTrackRect = true;
}

void *processVideoTrack(void *arg){
        // while(g_Frame != NULL && g_Size[0] != 0 && g_Size[1] != 0) {
        //         if(g_StopVideo) {
        //                 break;
        //         }

                float rateX = 1.0f;
                float rateY = 1.0f;
                int resizeWidth = g_Size[0];
                int resizeHeight = g_Size[1];
                if(g_Size[0] > 600 || g_Size[1] > 600){
                  rateX = 400*1.0f/g_Size[0];
                  rateY = 400*1.0f/g_Size[1];
                  if(rateX < rateY){
                    resizeWidth = 400;
                    resizeHeight = (int)(g_Size[1]*rateX);
                    rateY = rateX;
                  }else{
                    resizeHeight = 400;
                    resizeWidth = (int)(g_Size[0]*rateY);
                    rateX = rateY;
                  }
                }
                Mat img = Mat(g_Size[1],g_Size[0],CV_8UC3,(void*)g_Frame);
                Mat resultMat,tmpMat;
                #ifdef LIBCMT
                if(rateX == 1.0f && rateY == 1.0f){
                  cv::cvtColor(img,resultMat,CV_RGB2RGBA);
                }else{
                  cv::cvtColor(img, tmpMat, CV_RGB2RGBA);
                  cv::resize(tmpMat,resultMat,cv::Size(resizeWidth,resizeHeight));
                }
                #else
                if(rateX == 1.0f&& rateY == 1.0f){
                  cv::cvtColor(img,resultMat,CV_RGB2GRAY);
                }else{
                  cv::cvtColor(img, tmpMat, CV_RGB2GRAY);
                  cv::resize(tmpMat,resultMat,cv::Size(resizeWidth,resizeHeight));
                }
                #endif
                if(!CMTinitiated) {
                        pthread_mutex_lock(&mutex);
                        cv::Rect rect(g_VideoRect.x*rateX,g_VideoRect.y*rateY,g_VideoRect.width*rateX,g_VideoRect.y*rateY);
                        pthread_mutex_unlock(&mutex);
                        // ALOG("x:%d,y:%d,width:%d,height:%d,imagewidth:%d,imageHeight:%d",rect.x,rect.y,rect.width,rect.height,g_Size[0],g_Size[1]);
                #ifdef LIBCMT
                         libCMT->initialize(resultMat,rect);
                        CMTinitiated=true;
                #else
                        CMTinitiated = g_cmt->initialise(resultMat,Point2f(rect.x,rect.y),Point2f(rect.x+rect.width,rect.y+rect.height));
                #endif
                }else{
                #ifdef LIBCMT
                        RotatedRect resultRect;
                        libCMT->processFrame(resultMat);
                        if ( libCMT->_isMatched) {
                                resultRect = libCMT->_outRot;
                        } else {
                                resultRect = RotatedRect(Point2f(0,0),Size2f(0,0),0);
                        }
                        pthread_mutex_lock(&mutex);
                        g_VideoRect.x = resultRect.center.x - resultRect.size.width/2;
                        g_VideoRect.y = resultRect.center.y - resultRect.size.height/2;
                        g_VideoRect.width = resultRect.size.width;
                        g_VideoRect.height = resultRect.size.height;
                        pthread_mutex_unlock(&mutex);
               #else
                        g_cmt->processFrame(resultMat);
                        pthread_mutex_lock(&mutex);
                        g_VideoRect.x = g_cmt->topLeft.x;
                        g_VideoRect.y = g_cmt->topLeft.y;
                        g_VideoRect.width = g_cmt->bottomRight.x-g_cmt->topLeft.x;
                        g_VideoRect.height = g_cmt->bottomRight.y-g_cmt->topLeft.y;
                        pthread_mutex_unlock(&mutex);
              #endif
                }
                g_ProcessEnd = true;
        // }
}

JNIEXPORT jint JNICALL Java_com_melonteam_objtrack_ObjTrack_readFile(JNIEnv *env,jobject obj,jstring filePath){
        AVFormatContext *pFormatCtx = NULL;
        int i, videoStream;
        AVCodecContext  *pCodecCtx = NULL;
        AVCodec         *pCodec = NULL;
        AVFrame         *pFrame = NULL;
        AVFrame         *pFrameRGB = NULL;
        AVPacket packet;
        int frameFinished;
        int numBytes;
        uint8_t         *buffer = NULL;

        AVDictionary    *optionsDict = NULL;
        struct SwsContext      *sws_ctx = NULL;

        const char *nativeString = env->GetStringUTFChars(filePath, JNI_FALSE);
        ALOG("filePath:%s",nativeString);

        av_register_all();

        if(avformat_open_input(&pFormatCtx, nativeString, NULL, NULL)!=0)
                return -1;

        if(avformat_find_stream_info(pFormatCtx, NULL)<0)
                return -1;

        av_dump_format(pFormatCtx, 0, nativeString, 0);
        env->ReleaseStringUTFChars(filePath, nativeString);
        videoStream=-1;
        for(i=0; i<pFormatCtx->nb_streams; i++)
                if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
                        videoStream=i;
                        break;
                }
        if(videoStream==-1)
                return -1;
        pCodecCtx=pFormatCtx->streams[videoStream]->codec;
        AVStream * stream = pFormatCtx->streams[videoStream];
        int frame_rate = stream->avg_frame_rate.num/stream->avg_frame_rate.den;
        double frame_time = 1000000/frame_rate;

        pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
        if(pCodec==NULL) {
                fprintf(stderr, "Unsupported codec!\n");
                return -1;
        }
        if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
                return -1;

        pFrame=av_frame_alloc();

        pFrameRGB=av_frame_alloc();
        if(pFrameRGB==NULL)
                return -1;

        numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, pCodecCtx->width,
                                    pCodecCtx->height);
        buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

        sws_ctx =
                sws_getContext
                (
                        pCodecCtx->width,
                        pCodecCtx->height,
                        pCodecCtx->pix_fmt,
                        pCodecCtx->width,
                        pCodecCtx->height,
                        AV_PIX_FMT_RGB24,
                        SWS_BILINEAR,
                        NULL,
                        NULL,
                        NULL
                );

        avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                       pCodecCtx->width, pCodecCtx->height);

        i=0;
        uint8_t *frame = nullptr;
        jclass clazz = NULL;
        if(g_SizeArray == NULL) {
                g_SizeArray = env->NewIntArray(2);
        }
        g_StopVideo = false;
        g_PauseVideo = true;
        g_ByteArray = NULL;
        g_SizeArray = NULL;
        g_VideoRect = Rect(0,0,0,0);
        g_ProcessEnd = true;
        CMTinitiated = false;
        g_initTrackRect = false;
        while(av_read_frame(pFormatCtx, &packet)>=0) {
                if(g_StopVideo) {
                        break;
                }
                if(packet.stream_index==videoStream) {
                        double start_time = now_ms();
                        avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
                                              &packet);

                        if(frameFinished) {
                                sws_scale
                                (
                                        sws_ctx,
                                        (uint8_t const * const *)pFrame->data,
                                        pFrame->linesize,
                                        0,
                                        pCodecCtx->height,
                                        pFrameRGB->data,
                                        pFrameRGB->linesize
                                );
                                ++i;
                                g_Size[0] = pCodecCtx->width;
                                g_Size[1] = pCodecCtx->height;
                                int length = (pCodecCtx->width)*(pCodecCtx->height);
                                // pthread_mutex_lock(&mutex);
                                allocateFrameSize(env,length*3);
                                if(g_ByteArray == NULL) {
                                        g_ByteArray = env->NewByteArray(length*3);
                                        g_SizeArray = env->NewIntArray(2);
                                }
                                memcpy(g_Frame, pFrameRGB->data[0], length*3);
                                // pthread_mutex_unlock(&mutex);
                                env->SetByteArrayRegion(g_ByteArray, 0, g_FrameSize, (jbyte*)g_Frame);
                                env->SetIntArrayRegion(g_SizeArray,0,2,g_Size);
                                if(clazz == NULL) {
                                        clazz = env->FindClass("com/melonteam/objtrack/ObjTrack");
                                }
                                if(clazz != NULL) {
                                        //ToDO 以后改成非静态
                                        jmethodID videoCallBackMethod = env->GetStaticMethodID(clazz, "OnVideoCallBack", "([B[I)V");
                                        if(videoCallBackMethod == NULL) {
                                                ALOG("Cannot find OnVideoCallBack");
                                        }else{
                                                env->CallStaticVoidMethod(clazz,videoCallBackMethod,g_ByteArray,g_SizeArray);
                                        }
                                }
                                if(g_Size[0] != 0 && g_Size[1] != 0 && g_ProcessEnd &&g_initTrackRect) {
                                        pthread_t tid;
                                        pthread_create(&tid,NULL,processVideoTrack,(void *)NULL);
                                        g_ProcessEnd = false;
                                }else{
                                  ALOG("lost frame:%d",i);
                                }
                                // memcpy(g_Frame+length, pFrame->data[1], length/4);
                                // memcpy(g_Frame+length+length/4, pFrame->data[2], length/4);
                                // ALOG("i:%d,width:%d,height:%d",i,pCodecCtx->width,pCodecCtx->height);
                                // fwrite(pFrame->data[0],(pCodecCtx->width)*(pCodecCtx->height),1,output);
                                // fwrite(pFrame->data[1],(pCodecCtx->width)*(pCodecCtx->height)/4,1,output);
                                // fwrite(pFrame->data[2],(pCodecCtx->width)*(pCodecCtx->height)/4,1,output);
                                int sleepTime = (int)(frame_time - (now_ms() - start_time)*1000);
                                if(sleepTime > 0) {
                                        usleep(sleepTime);
                                }
                                while(g_PauseVideo) {
                                        if(g_StopVideo) {
                                                break;
                                        }
                                        usleep(200);
                                }
                        }

                }

                av_free_packet(&packet);
                if(g_StopVideo){
                    break;
                }
        }
        g_StopVideo = true;
        if(clazz != NULL) {
                env->DeleteLocalRef(clazz);
        }
        if(g_ByteArray != NULL) {
                env->DeleteLocalRef(g_ByteArray);
                env->DeleteLocalRef(g_SizeArray);
        }
        av_free(buffer);
        av_free(pFrameRGB);

        av_free(pFrame);

        avcodec_close(pCodecCtx);

        avformat_close_input(&pFormatCtx);
        return 0;
}


JNIEXPORT jbyteArray JNICALL Java_com_melonteam_objtrack_ObjTrack_openTrack(JNIEnv *env, jobject, jbyteArray yuvData,jint x, jint y, jint width, jint height,jint imageWidth,jint imageHeight)
{
        int len = env->GetArrayLength (yuvData);

        //ALOG("x:%d,y:%d,w:%d,h:%d,width:%d,height:%d,len:%d",x,y,width,height,imageWidth,imageHeight,len);
        char* buf = new char[len];
        env->GetByteArrayRegion (yuvData, 0, len, reinterpret_cast<jbyte*>(buf));
        Mat im_yuv = Mat(imageHeight+imageHeight/2,imageWidth,CV_8UC1,buf);
        Mat img,grayMat;
        cv::cvtColor(im_yuv, img, CV_YUV420sp2GRAY);
        cv::resize(img,grayMat,cv::Size(CmtWidth,CmtHeight));
        float rateX = CmtWidth*1.0f/imageWidth;
        float rateY = CmtHeight*1.0f/imageHeight;
        Point p1(x*rateX,y*rateY);
        Point p2((x+width)*rateX,(y+height)*rateY);
        Rect rect = Rect(p1,p2);
        #ifdef LIBCMT
        Mat rgbMat;
        cv::cvtColor(grayMat,rgbMat,CV_GRAY2RGBA);
        libCMT->initialize(rgbMat,rect);
        CMTinitiated=true;
        #else
        CMTinitiated = g_cmt->initialise(grayMat,p1,p2);
        #endif

}

JNIEXPORT jintArray JNICALL Java_com_melonteam_objtrack_ObjTrack_getTrackResult(JNIEnv *env,jobject){
        if (!CMTinitiated)
                return NULL;
        jintArray result = env->NewIntArray(8);
        jint fill[8];
        Rect rect = g_VideoRect;
        // float rateX = g_Size[0]*1.0f/CmtWidth;
        // float rateY = g_Size[1]*1.0f/CmtHeight;
        float rateX = 1.0f;
        float rateY = 1.0f;
        if(g_Size[0] > 600 || g_Size[1] > 600){
          rateX = g_Size[0]*1.0f/400;
          rateY = g_Size[1]*1.0f/400;
          if(rateX > rateY){
            rateY = rateX;
          }else{
            rateX = rateY;
          }
        }
        int width = rect.width*rateX;
        int height = rect.height*rateY;
        fill[0] = (int)rect.x*rateX;
        fill[1] = (int)rect.y*rateY;
        fill[2] = (int)rect.x*rateX+width;
        fill[3] = (int)rect.y*rateY;
        fill[4] = (int)rect.x*rateX;
        fill[5] = (int)rect.y*rateY+height;
        fill[6] = (int)rect.x*rateX+width;
        fill[7] = (int)rect.y*rateY+height;
        env->SetIntArrayRegion(result, 0, 8, fill);
        return result;

}

JNIEXPORT jintArray JNICALL Java_com_melonteam_objtrack_ObjTrack_processTrack(JNIEnv *env, jobject,jbyteArray yuvData,jint imageWidth,jint imageHeight)
{
        if (!CMTinitiated)
                return NULL;
        int len = env->GetArrayLength (yuvData);
        char* buf = new char[len];
        jintArray result = env->NewIntArray(8);
        Rect rect;
        env->GetByteArrayRegion (yuvData, 0, len, reinterpret_cast<jbyte*>(buf));
        Mat im_yuv = Mat(imageHeight+imageHeight/2,imageWidth,CV_8UC1,buf);
        Mat img,grayMat;
        cv::cvtColor(im_yuv, img, CV_YUV420sp2GRAY);
        cv::resize(img,grayMat,cv::Size(CmtWidth,CmtHeight));
#ifdef LIBCMT
        RotatedRect resultRect;
        Mat rgbMat;
        cv::cvtColor(grayMat,rgbMat,CV_GRAY2RGBA);

        libCMT->processFrame(rgbMat);
        if ( libCMT->_isMatched) {
                resultRect = libCMT->_outRot;
        } else {
                resultRect = RotatedRect(Point2f(0,0),Size2f(0,0),0);
        }
        rect.x = resultRect.center.x - resultRect.size.width/2;
        rect.y = resultRect.center.y - resultRect.size.height/2;
        rect.width = resultRect.size.width;
        rect.height = resultRect.size.height;
#else
        g_cmt->processFrame(grayMat);
        rect.x = g_cmt->topLeft.x;
        rect.y = g_cmt->topLeft.y;
        rect.width = g_cmt->bottomRight.x-g_cmt->topLeft.x;
        rect.height = g_cmt->bottomRight.y-g_cmt->topLeft.y;
#endif

        jint fill[8];

        float rateX = imageWidth*1.0f/CmtWidth;
        float rateY = imageHeight*1.0f/CmtHeight;
        int width = rect.width*rateX;
        int height = rect.height*rateY;
        fill[0] = (int)rect.x*rateX;
        fill[1] = (int)rect.y*rateY;
        fill[2] = (int)rect.x*rateX+width;
        fill[3] = (int)rect.y*rateY;
        fill[4] = (int)rect.x*rateX;
        fill[5] = (int)rect.y*rateY+height;
        fill[6] = (int)rect.x*rateX+width;
        fill[7] = (int)rect.y*rateY+height;
        env->SetIntArrayRegion(result, 0, 8, fill);
        return result;

}

}
