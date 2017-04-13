package com.melonteam.objtrack;

import java.util.ArrayList;
import java.util.List;

/**
 * Created by zhangpengyu on 2017/3/20.
 */

public class ObjTrack {

    /**
     * 传入数据格式
     * NV21 Android摄像头默认格式
     */
    public static final int TYPE_NV21 = 0;
    public static final int TYPE_RGB = 1;
    public static final int TYPE_RGBA = 2;

    private static class ObjTrackHolder{
        public static ObjTrack instance = new ObjTrack();
    }

    private ObjTrack(){}

    public static ObjTrack newInstance(){
        return ObjTrackHolder.instance;
    }

    public static void init() {
        try {
            System.loadLibrary("object_track");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public interface OnVideoFrameListener{
        public void onVideoFrame(byte[] videoData,int[] size);
    }

    public OnVideoFrameListener mOnVideoFrameListener;

    public native void openTrack(byte[] yuvData,int dataType, int x, int y, int w, int h, int cameraWidth, int cameraHeight);

    /**
     *
     * @param yuvData
     * @param dataType
     * @param cameraWidth
     * @param cameraHeight
     * @return topLeft,topRight,bottomLeft,bottomRight
     */
    public native int[] processTrack(byte[] yuvData, int dataType,int cameraWidth, int cameraHeight);

    public native void initVideo(String filePath);

    public native void releaseVideo();

    public native void pauseVideo();

    public native void playVideo();

    public void setVideoFrameListener(OnVideoFrameListener onVideoFrameListener){
        mOnVideoFrameListener = onVideoFrameListener;
    }

    //called by native
    public void OnVideoCallBack(byte[] videoData,int[] size){
        if(mOnVideoFrameListener != null){
            mOnVideoFrameListener.onVideoFrame(videoData,size);
        }
    }
}
