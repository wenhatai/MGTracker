#include <jni.h>

#include <unistd.h> //usleep
#include <time.h>
#include <pthread.h>

#include <android/log.h>

#define  LOG_TAG    "MediaDecode"
#define  ALOG(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

extern "C" {
  #include "libavcodec/avcodec.h"
  #include "libavformat/avformat.h"
  #include "libswscale/swscale.h"

uint8_t *g_Frame = NULL;
int g_FrameSize = 0;
pthread_mutex_t mutex;
jint *g_Size = new jint[2];
jbyteArray g_ByteArray = NULL;
jintArray g_SizeArray = NULL;
bool g_StopVideo = false;
bool g_PauseVideo = true;


// from android samples
/* return current time in milliseconds */
static double now_ms(void) {
        struct timespec res;
        clock_gettime(CLOCK_MONOTONIC, &res);
        return 1000.0 * res.tv_sec + (double) res.tv_nsec / 1e6;
}

void allocateFrameSize(int size){
        if(g_FrameSize >= size) {
                return;
        }
        if(g_Frame != NULL) {
                delete[] g_Frame;
                g_Frame = NULL;
        }
        g_Frame = new uint8_t[size];
        g_FrameSize = size;
}

JNIEXPORT jint JNICALL Java_com_melonteam_mgtracker_NativeMedia_releaseVideo(JNIEnv *env,jobject obj){
        g_StopVideo = true;
}

JNIEXPORT jint JNICALL Java_com_melonteam_mgtracker_NativeMedia_pauseVideo(JNIEnv *env,jobject obj){
        g_PauseVideo = true;
}

JNIEXPORT jint JNICALL Java_com_melonteam_mgtracker_NativeMedia_playVideo(JNIEnv *env,jobject obj){
        g_PauseVideo = false;
}

AVFormatContext *pFormatCtx = NULL;
int videoStream;
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


JNIEXPORT jint JNICALL Java_com_melonteam_mgtracker_NativeMedia_initVideo(JNIEnv *env,jobject obj,jstring filePath){
        const char *nativeString = env->GetStringUTFChars(filePath, JNI_FALSE);
        ALOG("initVideo:%s",nativeString);

        av_register_all();

        if(avformat_open_input(&pFormatCtx, nativeString, NULL, NULL)!=0)
                return -1;

        if(avformat_find_stream_info(pFormatCtx, NULL)<0)
                return -1;

        av_dump_format(pFormatCtx, 0, nativeString, 0);
        env->ReleaseStringUTFChars(filePath, nativeString);
        videoStream=-1;
        for(int i=0; i<pFormatCtx->nb_streams; i++)
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

        int frameIndex=0;
        uint8_t *frame = nullptr;
        jclass clazz = NULL;
        if(g_SizeArray == NULL) {
                g_SizeArray = env->NewIntArray(2);
        }
        g_StopVideo = false;
        g_PauseVideo = true;
        g_ByteArray = NULL;
        g_SizeArray = NULL;
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
                                ++frameIndex;
                                g_Size[0] = pCodecCtx->width;
                                g_Size[1] = pCodecCtx->height;
                                int length = (pCodecCtx->width)*(pCodecCtx->height);
                                // pthread_mutex_lock(&mutex);
                                allocateFrameSize(length*3);
                                if(g_ByteArray == NULL) {
                                        g_ByteArray = env->NewByteArray(length*3);
                                        g_SizeArray = env->NewIntArray(2);
                                }
                                memcpy(g_Frame, pFrameRGB->data[0], length*3);
                                // pthread_mutex_unlock(&mutex);
                                env->SetByteArrayRegion(g_ByteArray, 0, length*3, (jbyte*)g_Frame);
                                env->SetIntArrayRegion(g_SizeArray,0,2,g_Size);
                                if(clazz == NULL) {
                                        clazz = env->FindClass("com/melonteam/mgtracker/NativeMedia");
                                }
                                if(clazz != NULL) {
                                        //ToDO 以后改成非静态
                                        jmethodID videoCallBackMethod = env->GetMethodID(clazz, "OnVideoCallBack", "([B[I)V");
                                        if(videoCallBackMethod == NULL) {
                                                ALOG("Cannot find OnVideoCallBack");
                                        }else{
                                                env->CallVoidMethod(obj,videoCallBackMethod,g_ByteArray,g_SizeArray);
                                        }
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
                if(g_StopVideo) {
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
        if(buffer != NULL) {
                av_free(buffer);
                buffer = NULL;
        }
        if(pFrameRGB != NULL) {
                av_free(pFrameRGB);
                pFrameRGB = NULL;
        }
        if(pFrame != NULL) {
                av_free(pFrame);
                pFrame = NULL;
        }
        if(pCodecCtx != NULL) {
                avcodec_close(pCodecCtx);
                pCodecCtx = NULL;
        }
        if(pFormatCtx != NULL) {
                avformat_close_input(&pFormatCtx);
                pFormatCtx = NULL;
        }
        return 0;
}

}
