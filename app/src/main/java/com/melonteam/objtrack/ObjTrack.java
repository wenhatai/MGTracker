package com.melonteam.objtrack;

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

    public static void init() {
        try {
            System.loadLibrary("object_track");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static OnVideoFrameListener onVideoFrameListener;

    public static interface OnVideoFrameListener{
        public void onVideoFrame(byte[] videoData,int[] size);
    }

    public static native void openTrack(byte[] yuvData,int dataType, int x, int y, int w, int h, int cameraWidth, int cameraHeight);

    public static native int[] processTrack(byte[] yuvData, int dataType,int cameraWidth, int cameraHeight);

    public static native void initVideo(String filePath);

    public static native void releaseVideo();

    public static native void pauseVideo();

    public static native void playVideo();

    public static void OnVideoCallBack(byte[] videoData,int[] size){
        if(onVideoFrameListener != null){
            onVideoFrameListener.onVideoFrame(videoData,size);
        }
    }
}
