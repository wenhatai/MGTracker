package com.melonteam.mgtracker;


/**
 * Created by zhangpengyu on 2017/3/20.
 */

public class MGTracker {

    /**
     * 传入数据格式
     * NV21 Android摄像头默认格式
     */
    public static final int TYPE_NV21 = 0;
    public static final int TYPE_RGB = 1;
    public static final int TYPE_RGBA = 2;

    private static class ObjTrackHolder{
        public static MGTracker instance = new MGTracker();
    }

    private MGTracker(){
        init();
    }

    public static MGTracker newInstance(){
        return ObjTrackHolder.instance;
    }

    public static void init() {
        try {
            System.loadLibrary("mgtracker");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

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


}
