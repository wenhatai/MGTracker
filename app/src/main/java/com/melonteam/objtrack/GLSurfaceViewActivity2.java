package com.melonteam.objtrack;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.Window;
import android.view.WindowManager;

/**
 * Created by zhangpengyu on 2017/3/20.
 *
 * 直接preview效率很低，这份代码不用了。留着是为了以后查preview效率瓶颈的时候可能会用到
 */
@Deprecated
public class GLSurfaceViewActivity2 extends Activity {
    private MyGLSurfaceView2 mView;
    private PowerManager.WakeLock mWL;

    public static final int ScreentWidth = UtilMethod.getDeviceHeight();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // full screen & full brightness
        requestWindowFeature ( Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mWL = ((PowerManager)getSystemService ( Context.POWER_SERVICE )).newWakeLock(PowerManager.FULL_WAKE_LOCK, "WakeLock");
        mWL.acquire();
        setContentView(R.layout.activity_surfaceview2);
        mView = (MyGLSurfaceView2) findViewById(R.id.obj_surfaceview);
    }

    @Override
    protected void onPause() {
        if ( mWL.isHeld() )
            mWL.release();
        mView.onPause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mView.onResume();
        if(!mWL.isHeld()) mWL.acquire();
    }
}
