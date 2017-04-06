package com.melonteam.objtrack;

import android.app.Application;

/**
 * Created by zhangpengyu on 2017/3/20.
 */

public class AppAplication extends Application {

    @Override
    public void onCreate() {
        super.onCreate();
        ObjTrack.init();
    }
}
