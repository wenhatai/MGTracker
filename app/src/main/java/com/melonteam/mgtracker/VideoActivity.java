package com.melonteam.mgtracker;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.graphics.RectF;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by zhangpengyu on 2017/4/4.
 */

public class VideoActivity extends Activity {
    public static final String TAG = "VideoActivity";
    /**
     * Hold a reference to our GLSurfaceView
     */
    private GLSurfaceView mGLSurfaceView;
    private MyRenderer mRender;
    private String mFilePath;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video);
        mGLSurfaceView = (GLSurfaceView) findViewById(R.id.video_surfaceview);
        mFilePath = getIntent().getStringExtra("FilePath");
        File file = new File(mFilePath);
        if (!file.exists()) {
            Toast.makeText(this, "文件不存在", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }


        // Check if the system supports OpenGL ES 2.0.
        final ActivityManager activityManager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        final ConfigurationInfo configurationInfo = activityManager.getDeviceConfigurationInfo();
        final boolean supportsEs2 = configurationInfo.reqGlEsVersion >= 0x20000;

        if (supportsEs2) {
            // Request an OpenGL ES 2.0 compatible context.
            mGLSurfaceView.setEGLContextClientVersion(2);

            // Set the renderer to our demo renderer, defined below.
            mRender = new MyRenderer(this);
            mGLSurfaceView.setRenderer(mRender);
        } else {
            // This is where you could create an OpenGL ES 1.x compatible
            // renderer if you wanted to support both ES 1 and ES 2.
            return;
        }
    }

    @Override
    protected void onResume() {
        // The activity must call the GL surface view's onResume() on activity onResume().
        super.onResume();
        mGLSurfaceView.onResume();
    }

    @Override
    protected void onPause() {
        // The activity must call the GL surface view's onPause() on activity onPause().
        super.onPause();
        mGLSurfaceView.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mRender != null) {
            mRender.destroy();
        }
    }

    static class MyRenderer implements GLSurfaceView.Renderer, NativeMedia.OnVideoFrameListener, View.OnClickListener {
        private static final String VERTEX_SHADER = "uniform mat4 uMVPMatrix;" +
                "attribute vec4 vPosition;" +
                "attribute vec2 a_texCoord;" +
                "varying vec2 v_texCoord;" +
                "void main() {" +
                "  gl_Position = uMVPMatrix * vPosition;" +
                "  v_texCoord = a_texCoord;" +
                "}";


        private static final String FRAGMENT_SHADER = "#ifdef GL_ES                                       \n" +
                "precision highp float;                             \n" +
                "#endif                                             \n" +

                "varying vec2 v_texCoord;                           \n" +
                "uniform sampler2D y_texture;                       \n" +
                "uniform sampler2D u_texture;                      \n" +
                "uniform sampler2D v_texture;                      \n" +
                "uniform sampler2D rgb_texture;\n" +

                "void main (void){                                  \n" +
                "   float r, g, b, y, u, v;                         \n" +

                "r = texture2D(rgb_texture, v_texCoord).r;\n" +
                "g = texture2D(rgb_texture, v_texCoord).g;\n" +
                "b = texture2D(rgb_texture, v_texCoord).b;\n" +
                "   gl_FragColor = vec4(r, g, b, 1.0);              \n" +
                "}                                                  \n";
        private static final float[] VERTEX = {   // in counterclockwise order:
                1, 1, 0,   // top right
                -1, 1, 0,  // top left
                -1, -1, 0, // bottom left
                1, -1, 0,  // bottom right
        };
        private static final short[] VERTEX_INDEX = {0, 1, 2, 2, 0, 3};
        private static final float[] UV_TEX_VERTEX = {   // in clockwise order:
                1, 0,  // bottom right
                0, 0,  // bottom left
                0, 1,  // top left
                1, 1,  // top right
        };

        private final FloatBuffer mVertexBuffer;
        private final ShortBuffer mVertexIndexBuffer;
        private final FloatBuffer mUvTexVertexBuffer;

        private final float[] mProjectionMatrix = new float[16];
        private final float[] mCameraMatrix = new float[16];
        private final float[] mMVPMatrix = new float[16];
        private final float[] mModelMatrix = new float[16];

        private int mProgram;
        private int mPositionHandle;
        private int mMatrixHandle;
        private int mTexCoordHandle;
        private int mRGBTextSampleHandle;

        protected int mVideoWidth = 1;
        protected int mVideoHeight = 1;

        private ByteBuffer rgbBuffer;

        private int[] rgbTextureNames;
        VideoActivity mSurfaceViewActivity;
        DrawView mDrawView;
        TextView mInfoView;
        //        EditText mFilePathView;
        TextView mPlayBtn;
        boolean mIsPlaying = false;
        Handler mUiHandler = new Handler(Looper.getMainLooper());
        byte[] mCurrentData;
        private Boolean mIsInitTrack = false;

        long mDrawTimeStamp = 0;
        long mTrackTimeStamp = 0;
        int mDrawFrame = 0;
        int mTrackFrame = 0;
        int mDrawFps = 0;
        int mTrackFps = 0;


        MyRenderer(final VideoActivity activity) {
            this.mSurfaceViewActivity = activity;
            mVertexBuffer = ByteBuffer.allocateDirect(VERTEX.length * 4)
                    .order(ByteOrder.nativeOrder())
                    .asFloatBuffer()
                    .put(VERTEX);
            mVertexBuffer.position(0);

            mVertexIndexBuffer = ByteBuffer.allocateDirect(VERTEX_INDEX.length * 2)
                    .order(ByteOrder.nativeOrder())
                    .asShortBuffer()
                    .put(VERTEX_INDEX);
            mVertexIndexBuffer.position(0);

            mUvTexVertexBuffer = ByteBuffer.allocateDirect(UV_TEX_VERTEX.length * 4)
                    .order(ByteOrder.nativeOrder())
                    .asFloatBuffer()
                    .put(UV_TEX_VERTEX);
            mUvTexVertexBuffer.position(0);

            rgbBuffer = ByteBuffer.allocate(mVideoWidth * mVideoHeight * 3).order(ByteOrder.nativeOrder());
            mPlayBtn = (Button) mSurfaceViewActivity.findViewById(R.id.video_play);
//            mFilePathView = (EditText) mSurfaceViewActivity.findViewById(R.id.video_filepath);
//            mFilePathView.setText(mFilePath);
            mDrawView = (DrawView) mSurfaceViewActivity.findViewById(R.id.video_drawview);
            mInfoView = (TextView) mSurfaceViewActivity.findViewById(R.id.video_infoview);
            mDrawView.mViewMode = DrawView.VideoMode;
            mDrawView.setDrawCallback(new DrawView.CaluateViewCallBack() {
                @Override
                public void onCaluate(RectF rectF) {
                    mIsInitTrack = false;
                }
            });
            mPlayBtn.setOnClickListener(this);
            mSurfaceViewActivity.findViewById(R.id.video_restart).setOnClickListener(this);

        }

        void initView() {
            int screenHeight = UtilMethod.getDeviceHeight();
            int screenWidth = UtilMethod.getDeviceWidth();
            float rateScreen = screenHeight * 1.0f / screenWidth;
            float rateVideo = mVideoHeight * 1.0f / mVideoWidth;
            int textureWidth;
            int textureHeight;
            if (rateScreen > rateVideo) {
                textureWidth = screenWidth;
                textureHeight = screenWidth * mVideoHeight / mVideoWidth;
            } else {
                textureHeight = screenHeight;
                textureWidth = screenHeight * mVideoWidth / mVideoHeight;
            }
            RelativeLayout.LayoutParams surfaceLayoutParams = (RelativeLayout.LayoutParams) mSurfaceViewActivity.mGLSurfaceView.getLayoutParams();
            surfaceLayoutParams.width = textureWidth;
            surfaceLayoutParams.height = textureHeight;
            mSurfaceViewActivity.mGLSurfaceView.setLayoutParams(surfaceLayoutParams);
            RelativeLayout.LayoutParams drawLayoutParams = (RelativeLayout.LayoutParams) mDrawView.getLayoutParams();
            drawLayoutParams.width = textureWidth;
            drawLayoutParams.height = textureHeight;
            mDrawView.setLayoutParams(drawLayoutParams);
        }


        @Override
        public void onSurfaceCreated(GL10 unused, EGLConfig config) {

            mProgram = GLES20.glCreateProgram();
            int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, VERTEX_SHADER);
            int fragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
            GLES20.glAttachShader(mProgram, vertexShader);
            GLES20.glAttachShader(mProgram, fragmentShader);
            GLES20.glLinkProgram(mProgram);

            mPositionHandle = GLES20.glGetAttribLocation(mProgram, "vPosition");
            mTexCoordHandle = GLES20.glGetAttribLocation(mProgram, "a_texCoord");
            mMatrixHandle = GLES20.glGetUniformLocation(mProgram, "uMVPMatrix");
            mRGBTextSampleHandle = GLES20.glGetUniformLocation(mProgram, "rgb_texture");

            GLES20.glEnable(GLES20.GL_TEXTURE_2D);
            rgbTextureNames = new int[1];
            GLES20.glGenTextures(1, rgbTextureNames, 0);
            NativeMedia.newInstance().setVideoFrameListener(this);

            restartVideo();
        }

        public void restartVideo() {
//            mFilePath = mFilePathView.getText().toString();
            File file = new File(mSurfaceViewActivity.mFilePath);
            if (!file.exists()) {
                mUiHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(mSurfaceViewActivity, "文件不存在", Toast.LENGTH_SHORT).show();
                    }
                });
                return;
            }
            NativeMedia.newInstance().releaseVideo();
            new Thread() {
                @Override
                public void run() {
                    Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
                    try {
                        Thread.sleep(200);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    mIsInitTrack = false;
                    mIsPlaying = false;
                    if (mDrawView.mDrawRectF != null) {
                        synchronized (mDrawView.mDrawRectF){
                            mDrawView.mDrawRectF = null;
                            mDrawView.mRectF = null;
                        }
                    }
                    mDrawView.postInvalidate();
                    NativeMedia.newInstance().initVideo(mSurfaceViewActivity.mFilePath);
                }
            }.start();

        }

        @Override
        public void onSurfaceChanged(GL10 unused, int width, int height) {
            Log.d(TAG, "width:" + width + "height:" + height);

            GLES20.glViewport(0, 0, width, height);
            final float ratio = (float) width / height;

            Matrix.frustumM(mProjectionMatrix, 0, -1, 1, -1, 1, 3, 7);

//            Matrix.setIdentityM(mModelMatrix, 0);
//            Matrix.translateM(mModelMatrix, 0, 0, 0, 0f);
////            Matrix.rotateM(mModelMatrix, 0, -90, 0f, 0f, 1f);
//
//            Matrix.frustumM(mProjectionMatrix, 0, -ratio, ratio, -1, 1, 2, 10f);
//            Matrix.setLookAtM(mCameraMatrix, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0);
//            Matrix.multiplyMM(mMVPMatrix, 0, mCameraMatrix, 0, mModelMatrix, 0);
//            Matrix.multiplyMM(mMVPMatrix, 0, mProjectionMatrix, 0, mMVPMatrix, 0);
        }

        @Override
        public void onDrawFrame(GL10 unused) {
            Matrix.setLookAtM(mCameraMatrix, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0);
            Matrix.multiplyMM(mMVPMatrix, 0, mProjectionMatrix, 0, mCameraMatrix, 0);

            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

            GLES20.glUseProgram(mProgram);

            GLES20.glEnableVertexAttribArray(mPositionHandle);
            GLES20.glVertexAttribPointer(mPositionHandle, 3, GLES20.GL_FLOAT, false, 0,
                    mVertexBuffer);

            GLES20.glEnableVertexAttribArray(mTexCoordHandle);
            GLES20.glVertexAttribPointer(mTexCoordHandle, 2, GLES20.GL_FLOAT, false, 0,
                    mUvTexVertexBuffer);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, rgbTextureNames[0]);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGB, mVideoWidth, mVideoHeight, 0,
                    GLES20.GL_RGB, GLES20.GL_UNSIGNED_BYTE, rgbBuffer);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,
                    GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,
                    GLES20.GL_REPEAT);
            GLES20.glUniform1i(mRGBTextSampleHandle, 0);

            GLES20.glUniformMatrix4fv(mMatrixHandle, 1, false, mMVPMatrix, 0);

            GLES20.glDrawElements(GLES20.GL_TRIANGLES, VERTEX_INDEX.length,
                    GLES20.GL_UNSIGNED_SHORT, mVertexIndexBuffer);

            GLES20.glDisableVertexAttribArray(mPositionHandle);
            GLES20.glDisableVertexAttribArray(mTexCoordHandle);

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

        void destroy() {
            GLES20.glDeleteTextures(1, rgbTextureNames, 0);
            NativeMedia.newInstance().setVideoFrameListener(null);
            NativeMedia.newInstance().releaseVideo();
        }

        static int loadShader(int type, String shaderCode) {
            int shader = GLES20.glCreateShader(type);
            GLES20.glShaderSource(shader, shaderCode);
            GLES20.glCompileShader(shader);
            return shader;
        }

        @Override
        public void onVideoFrame(byte[] videoData, int[] size) {
            if (mVideoWidth != size[0] || mVideoHeight != size[1]) {
                mVideoWidth = size[0];
                mVideoHeight = size[1];
                mDrawView.setCameraSize(mVideoWidth, mVideoHeight);
                mUiHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        initView();
                    }
                });
                rgbBuffer = ByteBuffer.allocate(mVideoWidth * mVideoHeight * 3).order(ByteOrder.nativeOrder());
            }
            mCurrentData = videoData;
            rgbBuffer.put(videoData, 0, mVideoWidth * mVideoHeight * 3).position(0);
            if (mDrawView.mDrawRectF != null ) {
                if(mTrackThread == null || !mTrackThread.isAlive()){
                    mTrackThread = new TrackThread();
                    mTrackThread.start();
                }else{
//                    Log.d(TAG,"lost frame");
                }

            }
//            int[] trackResult = ObjTrack.getTrackResult();
//            if(trackResult != null && mDrawView.mDrawRectF != null){
//                float rateY = mDrawView.getHeight() * 1.0f / mVideoHeight;
//                float rateX = mDrawView.getWidth() * 1.0f / mVideoWidth;
//                mDrawView.mDrawRectF.left = trackResult[0]*rateX;
//                mDrawView.mDrawRectF.top = trackResult[1] * rateY;
//                mDrawView.mDrawRectF.right = trackResult[6] * rateX;
//                mDrawView.mDrawRectF.bottom = trackResult[7] * rateY;
//                mDrawView.postInvalidate();
//            }
        }

        @Override
        public void onClick(View v) {
            switch (v.getId()) {
                case R.id.video_play:
                    if (mIsPlaying) {
                        NativeMedia.newInstance().pauseVideo();
                        mPlayBtn.setText("播放");
                    } else {
                        NativeMedia.newInstance().playVideo();
                        mPlayBtn.setText("暂停");
                    }
                    mIsPlaying = !mIsPlaying;
                    break;
                case R.id.video_restart:
                    restartVideo();
                    mPlayBtn.setText("播放");
                    break;
                default:
                    break;
            }
        }

        protected void updateFps() {
            mUiHandler.post(new Runnable() {
                @Override
                public void run() {
                    if(mIsInitTrack){
//                        String msg = MGTracker.newInstance().getDebugInfo();
                        mInfoView.setText("绘制fps:"+mDrawFps+",追踪fps:"+mTrackFps);
                    }else{
                        mInfoView.setText("绘制fps:"+mDrawFps);
                    }
                }
            });
        }

        TrackThread mTrackThread;

        class TrackThread extends Thread {
            @Override
            public void run() {
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
                long startTimeStamp = System.currentTimeMillis();
                mTrackFrame++;
                if (mDrawView.mDrawRectF == null || mCurrentData == null) {
                    return;
                }
                synchronized (mDrawView.mDrawRectF) {
                    if (mSurfaceViewActivity.isFinishing()) {
                        return;
                    }
                    float rateY = mDrawView.getHeight() * 1.0f / mVideoHeight;
                    float rateX = mDrawView.getWidth() * 1.0f / mVideoWidth;
                    RectF rectF = new RectF(mDrawView.mDrawRectF);
                    rectF.left = rectF.left / rateX;
                    rectF.right = rectF.right / rateX;
                    rectF.top = rectF.top / rateY;
                    rectF.bottom = rectF.bottom / rateY;
                    if (!mIsInitTrack) {
                        MGTracker.newInstance().openTrack(mCurrentData, MGTracker.TYPE_RGB, (int) rectF.left, (int) rectF.top, (int) rectF.width(), (int) rectF.height(), mVideoWidth, mVideoHeight);
                        mIsInitTrack = true;
                    } else {
                        int[] cmtData = MGTracker.newInstance().processTrack(mCurrentData, MGTracker.TYPE_RGB, mVideoWidth, mVideoHeight);
                        if (cmtData != null && mDrawView.mDrawRectF != null) {
                            mDrawView.mDrawRectF.left = cmtData[0] * rateX;
                            mDrawView.mDrawRectF.top = cmtData[1] * rateY;
                            mDrawView.mDrawRectF.right = cmtData[6] * rateX;
                            mDrawView.mDrawRectF.bottom = cmtData[7] * rateY;

                        }
                        int[] predictRectf = MGTracker.newInstance().getPredictTrack(mVideoWidth, mVideoHeight);
                        if (predictRectf != null && mDrawView.mPredictRectF != null) {
                            mDrawView.mPredictRectF.left = predictRectf[0] * rateX;
                            mDrawView.mPredictRectF.top = predictRectf[1] * rateY;
                            mDrawView.mPredictRectF.right = predictRectf[6] * rateX;
                            mDrawView.mPredictRectF.bottom = predictRectf[7] * rateY;
                        }
                        mDrawView.postInvalidate();
                    }
                }
                mTrackTimeStamp += (System.currentTimeMillis() - startTimeStamp);
                if (mTrackFrame >= 15) {
                    mTrackFrame = 0;
                    mTrackFps = (int) (15 * 1000 / mTrackTimeStamp);
                    mTrackTimeStamp = 0;
                    updateFps();
                }
            }
        }
    }

}
