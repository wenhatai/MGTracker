package com.melonteam.mgtracker;

import android.content.res.Resources;


/**
 * Created by zhangpengyu on 2017/4/13.
 */

public class UtilMethod {

    public static int getDeviceWidth() {
        return Resources.getSystem().getDisplayMetrics().widthPixels;
    }


    public static int getDeviceHeight() {
        return Resources.getSystem().getDisplayMetrics().heightPixels;
    }

}
