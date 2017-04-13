package com.melonteam.objtrack;

import android.content.res.Resources;

/**
 * Created by zhangpengyu on 2017/4/13.
 */

public class UtilMethod {

    public static int getScreenWidth() {
        return Resources.getSystem().getDisplayMetrics().widthPixels;
    }

    public static int getScreenHeight() {
        return Resources.getSystem().getDisplayMetrics().heightPixels;
    }
}
