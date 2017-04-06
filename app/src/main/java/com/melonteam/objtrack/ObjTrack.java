package com.melonteam.objtrack;

/**
 * Created by zhangpengyu on 2017/3/20.
 */

public class ObjTrack {

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

    public static native void openTrack(byte[] yuvData, int x, int y, int w, int h, int cameraWidth, int cameraHeight);

    public static native int[] processTrack(byte[] yuvData, int cameraWidth, int cameraHeight);

    public static native void readFile(String filePath);

    public static native void releaseVideo();

    public static native void pauseVideo();

    public static native void playVideo();

    //left,top,right,bottom
    public static native void setVideoTrack(int[] trackArea);

    public static native int[] getTrackResult();

//    public static native byte[] getYUVData(int[] size);

    public static void OnVideoCallBack(byte[] videoData,int[] size){
        if(onVideoFrameListener != null){
            onVideoFrameListener.onVideoFrame(videoData,size);
        }
    }
}
