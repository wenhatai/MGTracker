package com.melonteam.objtrack;

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

public class CameraActivity extends AppCompatActivity implements TextureView.SurfaceTextureListener, Camera.PreviewCallback {
    private Camera mCamera;
    private TextureView mTextureView;
    private DrawView mDrawView;
    private TextView mTextView;

    private int mCameraWidth;
    private int mCameraHeight;
    private byte[] mPreviewFrame1;
    private byte[] mPreviewFrame2;
    public boolean mSupportUserBuffer = true;

    private Boolean isInitTrack = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);
        mTextureView = (TextureView) findViewById(R.id.obj_textureview);
        mTextureView.setSurfaceTextureListener(this);
        mDrawView = (DrawView) findViewById(R.id.obj_drawview);
        mTextView = (TextView) findViewById(R.id.obj_infoview);
    }

    private void initView() {
        int textureWidth = UtilMethod.getScreenWidth();
        int textureHeight = UtilMethod.getScreenWidth() * mCameraWidth / mCameraHeight;
        mTextureView.setLayoutParams(new RelativeLayout.LayoutParams(
                textureWidth, textureHeight));
        mDrawView.setLayoutParams(new RelativeLayout.LayoutParams(textureWidth, textureHeight));
        mDrawView.setCameraSize(mCameraWidth, mCameraHeight);
        mDrawView.setDrawCallback(new DrawView.CaluateViewCallBack() {
            @Override
            public void onCaluate(RectF rectF) {
                isInitTrack = false;
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
    long mTrackTimeStamp = 0;
    int mDrawFrame = 0;
    int mTrackFrame = 0;
    int mDrawFps = 0;
    int mTrackFps = 0;

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
            mDrawFps = (int) (30 * 1000 / (currentTimeStamp - mDrawTimeStamp));
            mDrawTimeStamp = currentTimeStamp;
            updateFps();
        }
    }

    protected void updateFps(){
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if(isInitTrack){
                    mTextView.setText("绘制fps:"+mDrawFps+",追踪fps:"+mTrackFps);
                }else{
                    mTextView.setText("绘制fps:"+mDrawFps);
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
            long startTimeStamp = System.currentTimeMillis();
            mTrackFrame++;
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
                if (!isInitTrack) {
                    ObjTrack.newInstance().openTrack(mCurrentData, ObjTrack.TYPE_NV21,(int) rectF.left, (int) rectF.top, (int) rectF.width(), (int) rectF.height(), mCameraWidth, mCameraHeight);
                    isInitTrack = true;
                } else {
                    int[] cmtData = ObjTrack.newInstance().processTrack(mCurrentData,ObjTrack.TYPE_NV21,mCameraWidth, mCameraHeight);
                    if(cmtData != null && mDrawView.mDrawRectF != null){
                        mDrawView.mDrawRectF.left = cmtData[0] * rateX;
                        mDrawView.mDrawRectF.top = cmtData[1] * rateY;
                        mDrawView.mDrawRectF.right = cmtData[6] * rateX;
                        mDrawView.mDrawRectF.bottom = cmtData[7] * rateY;
                        mDrawView.postInvalidate();
                    }
                }
            }
            mTrackTimeStamp += (System.currentTimeMillis() - startTimeStamp);
            if (mTrackFrame >= 30) {
                mTrackFrame = 0;
                mTrackFps = (int) (30 * 1000 / mTrackTimeStamp);
                mTrackTimeStamp = 0;
                updateFps();
            }
        }
    }

    TrackThread mTrackThread;

//    Thread trackThread = new Thread(){
//        @Override
//        public void run() {
//            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
//            while (currentData != null){
//                long startTimeStamp = System.currentTimeMillis();
//                mTrackFrame++;
//                if (mDrawView.mDrawRectF == null) {
//                    return;
//                }
//                synchronized (mDrawView.mDrawRectF) {
//                    if(!mTextureView.isAvailable()){
//                        return;
//                    }
//                    float rateY = mDrawView.getWidth() * 1.0f / mCameraHeight;
//                    float rateX = mDrawView.getHeight() * 1.0f / mCameraWidth;
//                    RectF rectF = new RectF(mDrawView.mDrawRectF);
//                    rectF.left = rectF.left / rateX;
//                    rectF.right = rectF.right / rateX;
//                    rectF.top = rectF.top / rateY;
//                    rectF.bottom = rectF.bottom / rateY;
//                    if (!isInitTrack) {
//                        ObjTrack.openTrack(currentData, ObjTrack.TYPE_NV21,(int) rectF.left, (int) rectF.top, (int) rectF.width(), (int) rectF.height(), mCameraWidth, mCameraHeight);
//                        isInitTrack = true;
//                    } else {
//                        int[] cmtData = ObjTrack.processTrack(currentData,ObjTrack.TYPE_NV21,mCameraWidth, mCameraHeight);
//                        if(cmtData != null && mDrawView.mDrawRectF != null){
//                            mDrawView.mDrawRectF.left = cmtData[0] * rateX;
//                            mDrawView.mDrawRectF.top = cmtData[1] * rateY;
//                            mDrawView.mDrawRectF.right = cmtData[6] * rateX;
//                            mDrawView.mDrawRectF.bottom = cmtData[7] * rateY;
//                            mDrawView.postInvalidate();
//                        }
//                    }
//                }
//                mTrackTimeStamp += (System.currentTimeMillis() - startTimeStamp);
//                if (mTrackFrame >= 30) {
//                    mTrackFrame = 0;
//                    mTrackFps = (int) (30 * 1000 / mTrackTimeStamp);
//                    mTrackTimeStamp = 0;
//                    updateFps();
//                }
//            }
//        }
//    };

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
