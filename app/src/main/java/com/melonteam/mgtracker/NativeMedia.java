package com.melonteam.mgtracker;

/**
 * Created by zhangpengyu on 2017/4/13.
 */

public class NativeMedia {

    private static class NativeMediaHolder{
        public static NativeMedia instance = new NativeMedia();
    }

    private NativeMedia(){
        init();
    }

    public static NativeMedia newInstance(){
        return NativeMedia.NativeMediaHolder.instance;
    }

    public static void init() {
        try {
            System.loadLibrary("nativemedia");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public interface OnVideoFrameListener{
        public void onVideoFrame(byte[] videoData,int[] size);
    }

    public OnVideoFrameListener mOnVideoFrameListener;

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
