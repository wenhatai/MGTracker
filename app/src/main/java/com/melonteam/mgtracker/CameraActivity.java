package com.melonteam.mgtracker;

import android.graphics.ImageFormat;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.support.v7.app.AppCompatActivity;
import android.view.TextureView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.io.IOException;

/**
 * Created by zhangpengyu on 2017/4/4.
 */

public class CameraActivity extends AppCompatActivity implements TextureView.SurfaceTextureListener, Camera.PreviewCallback {
    private Camera mCamera;
    private TextureView mTextureView;
    private DrawView mDrawView;
    private TextView mInfoView;

    private int mCameraWidth;
    private int mCameraHeight;
    private byte[] mPreviewFrame1;
    private byte[] mPreviewFrame2;
    public boolean mSupportUserBuffer = true;

    private Boolean mIsInitTrack = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_camera);
        mTextureView = (TextureView) findViewById(R.id.obj_textureview);
        mTextureView.setSurfaceTextureListener(this);
        mDrawView = (DrawView) findViewById(R.id.obj_drawview);
        mInfoView = (TextView) findViewById(R.id.obj_infoview);
    }

    private void initView() {
        int textureWidth = UtilMethod.getDeviceWidth();
        int textureHeight = UtilMethod.getDeviceWidth() * mCameraWidth / mCameraHeight;
        mTextureView.setLayoutParams(new RelativeLayout.LayoutParams(
                textureWidth, textureHeight));
        mDrawView.setLayoutParams(new RelativeLayout.LayoutParams(textureWidth, textureHeight));
        mDrawView.setCameraSize(mCameraWidth, mCameraHeight);
        mDrawView.setDrawCallback(new DrawView.CaluateViewCallBack() {
            @Override
            public void onCaluate(RectF rectF) {
                mIsInitTrack = false;
            }
        });
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        mCamera = android.hardware.Camera.open();

        Camera.Parameters parameters = mCamera.getParameters();
        for (Camera.Size size : parameters.getSupportedPreviewSizes()) {
            if (size.width <= 800 && size.height <= 800) {
                mCameraHeight = size.height;
                mCameraWidth = size.width;
                break;
            }
        }
        initView();

        parameters.setPreviewSize(mCameraWidth, mCameraHeight);
        mCamera.setDisplayOrientation(90);
        mCamera.setParameters(parameters);
        int bitpixel = ImageFormat.getBitsPerPixel(parameters.getPreviewFormat());
        float byteNum = bitpixel * 1.0f/Byte.SIZE;
        int bufSize = (int)(mCameraHeight * mCameraWidth * byteNum);

        try {
            mCamera.setPreviewTexture(surface);
        } catch (IOException t) {
        }

        if(mSupportUserBuffer){
            mPreviewFrame1 = new byte[bufSize];
            mPreviewFrame2 = new byte[bufSize];
            mCamera.addCallbackBuffer(mPreviewFrame1);
            mCamera.addCallbackBuffer(mPreviewFrame2);
            mCamera.setPreviewCallbackWithBuffer(this);
        }else{
            mCamera.setPreviewCallback(this);
        }
        mCamera.startPreview();

    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
        // Ignored, the Camera does all the work for us
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        mCamera.setPreviewCallback(null);
        mCamera.stopPreview();
        mCamera.release();
        return true;
    }

    long mDrawTimeStamp = 0;
    int mDrawFrame = 0;
    float mDrawFps = 0;

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        // Update your view here!
        if(mDrawTimeStamp == 0){
            mDrawTimeStamp = System.currentTimeMillis();
        }
        mDrawFrame++;
        if (mDrawFrame >= 30) {
            mDrawFrame = 0;
            long currentTimeStamp = System.currentTimeMillis();
            mDrawFps =  (30 * 1000.0f / (currentTimeStamp - mDrawTimeStamp));
            mDrawTimeStamp = currentTimeStamp;
            updateFps();
        }
    }

    protected void updateFps(){
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                MGTracker.DebugInfo debugInfo = MGTracker.newInstance().getDebugInfo();
                if(debugInfo != null && mIsInitTrack){
                    String logInfo = String.format("绘制帧率:%.2f 追踪帧率:%.2f 跟踪耗时%.2f \n缩放：%.2f 密度: %d 结果：%.1f 预测结果：%d\n特征点:活动:%d|目标:%d|背景:%d|预测:%d|RDTD:%d",
                            mDrawFps,debugInfo.trackFrame,debugInfo.trackCost,debugInfo.trackScale,debugInfo.trackDensity,debugInfo.matchPercent,debugInfo.isMatch,
                            debugInfo.activePoints,debugInfo.targetPoints,debugInfo.framePoints,debugInfo.predictPoints,debugInfo.rdtdCount);
                    mInfoView.setText(logInfo);
                }else{
                    mInfoView.setText("绘制fps:"+mDrawFps);
                }
            }
        });
    }

    byte[] mCurrentData;
    Handler mHandler = new Handler(Looper.getMainLooper());

    class TrackThread extends Thread{
        @Override
        public void run() {
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
            if (mDrawView.mDrawRectF == null || mCurrentData == null) {
                return;
            }
            synchronized (mDrawView.mDrawRectF) {
                if(!mTextureView.isAvailable()){
                    return;
                }
                float rateY = mDrawView.getWidth() * 1.0f / mCameraHeight;
                float rateX = mDrawView.getHeight() * 1.0f / mCameraWidth;
                RectF rectF = new RectF(mDrawView.mDrawRectF);
                rectF.left = rectF.left / rateX;
                rectF.right = rectF.right / rateX;
                rectF.top = rectF.top / rateY;
                rectF.bottom = rectF.bottom / rateY;
                if (!mIsInitTrack) {
                    MGTracker.newInstance().openTrack(mCurrentData, MGTracker.TYPE_NV21,(int) rectF.left, (int) rectF.top, (int) rectF.width(), (int) rectF.height(), mCameraWidth, mCameraHeight);
                    mIsInitTrack = true;
                } else {
                    int[] cmtData = MGTracker.newInstance().processTrack(mCurrentData, MGTracker.TYPE_NV21,mCameraWidth, mCameraHeight);
                    if(cmtData != null && mDrawView.mDrawRectF != null){
                        mDrawView.mDrawRectF.left = cmtData[0] * rateX;
                        mDrawView.mDrawRectF.top = cmtData[1] * rateY;
                        mDrawView.mDrawRectF.right = cmtData[6] * rateX;
                        mDrawView.mDrawRectF.bottom = cmtData[7] * rateY;
                        mDrawView.postInvalidate();
                    }
                }
            }
        }
    }

    TrackThread mTrackThread;

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        try{
            if (mDrawView.mDrawRectF == null) {
                return;
            }
            mCurrentData = data;
            if(mTrackThread == null || !mTrackThread.isAlive()){
                mTrackThread = new TrackThread();
                mTrackThread.start();
            }
        }finally {
            if(mSupportUserBuffer){
                mCamera.addCallbackBuffer(data);
            }
        }
    }
}
