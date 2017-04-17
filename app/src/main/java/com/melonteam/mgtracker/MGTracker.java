package com.melonteam.mgtracker;


import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;

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


    public native int[] getPredictTrack(int cameraWidth, int cameraHeight);

    public native String getDebugJson();

    public DebugInfo getDebugInfo(){
         String json = getDebugJson();
        if(TextUtils.isEmpty(json)){
            return null;
        }
        try {
            JSONObject jsonObject = new JSONObject(json);

            DebugInfo debugInfo = new DebugInfo();
            debugInfo.trackFrame = (float) jsonObject.optDouble("trackFrame");
            debugInfo.trackCost = (float) jsonObject.optDouble("trackCost");
            debugInfo.trackScale = (float)jsonObject.optDouble("trackScale");
            debugInfo.trackDensity = jsonObject.optLong("trackDensity");
            debugInfo.matchPercent = (float) jsonObject.optDouble("matchPercent");
            debugInfo.isMatch = jsonObject.optInt("isMatch");
            debugInfo.activePoints = jsonObject.optInt("activePoints");
            debugInfo.targetPoints = jsonObject.optInt("targetPoints");
            debugInfo.framePoints = jsonObject.optInt("framePoints");
            debugInfo.predictPoints = jsonObject.optInt("predictPoints");
            debugInfo.rdtdCount = jsonObject.optInt("rdtdCount");
            return debugInfo;
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return null;
    }

    public static class DebugInfo{
        public float trackFrame;
        public float trackCost;
        public float trackScale;
        public long trackDensity;
        public float matchPercent;
        public int isMatch;
        public int activePoints;
        public int targetPoints;
        public int framePoints;
        public int predictPoints;
        public int rdtdCount;
    }

}
