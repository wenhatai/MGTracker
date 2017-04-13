package com.melonteam.objtrack;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.TextureView;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by zhangpengyu on 2017/3/31.
 */

public class SurfaceViewActivity extends Activity {

    /** Hold a reference to our GLSurfaceView */
    private GLSurfaceView mGLSurfaceView;
    private TextureView mUnusedTextureView;
    MyRenderer mRender;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_surfaceview);
        mGLSurfaceView = (GLSurfaceView) findViewById(R.id.obj_surfaceview);
        mUnusedTextureView = (TextureView) findViewById(R.id.unused_texture);

        // Check if the system supports OpenGL ES 2.0.
        final ActivityManager activityManager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        final ConfigurationInfo configurationInfo = activityManager.getDeviceConfigurationInfo();
        final boolean supportsEs2 = configurationInfo.reqGlEsVersion >= 0x20000;

        if (supportsEs2)
        {
            // Request an OpenGL ES 2.0 compatible context.
            mGLSurfaceView.setEGLContextClientVersion(2);

            // Set the renderer to our demo renderer, defined below.
            mRender = new MyRenderer(this);
            mGLSurfaceView.setRenderer(mRender);
        }
        else
        {
            // This is where you could create an OpenGL ES 1.x compatible
            // renderer if you wanted to support both ES 1 and ES 2.
            return;
        }
    }

    @Override
    protected void onResume()
    {
        // The activity must call the GL surface view's onResume() on activity onResume().
        super.onResume();
        mGLSurfaceView.onResume();
    }

    @Override
    protected void onPause()
    {
        // The activity must call the GL surface view's onPause() on activity onPause().
        super.onPause();
        mGLSurfaceView.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if(mRender != null){
            mRender.destroy();
        }
    }

    static class MyRenderer implements GLSurfaceView.Renderer ,Camera.PreviewCallback{
        private static final String VERTEX_SHADER = "uniform mat4 uMVPMatrix;" +
                "attribute vec4 vPosition;" +
                "attribute vec2 a_texCoord;" +
                "varying vec2 v_texCoord;" +
                "void main() {" +
                "  gl_Position = uMVPMatrix * vPosition;" +
                "  v_texCoord = a_texCoord;" +
                "}";
//        private static final String FRAGMENT_SHADER = "precision mediump float;" +
//                "varying vec2 v_texCoord;" +
//                "uniform sampler2D s_texture;" +
//                "void main() {" +
//                "  gl_FragColor = texture2D( s_texture, v_texCoord );" +
//                "}";



        private static final String FRAGMENT_SHADER = "#ifdef GL_ES                                       \n" +
                "precision highp float;                             \n" +
                "#endif                                             \n" +

                "varying vec2 v_texCoord;                           \n" +
                "uniform sampler2D y_texture;                       \n" +
                "uniform sampler2D uv_texture;                      \n" +

                "void main (void){                                  \n" +
                "   float r, g, b, y, u, v;                         \n" +

                //We had put the Y values of each pixel to the R,G,B components by GL_LUMINANCE,
                //that's why we're pulling it from the R component, we could also use G or B
                "   y = texture2D(y_texture, v_texCoord).r;         \n" +

                //We had put the U and V values of each pixel to the A and R,G,B components of the
                //texture respectively using GL_LUMINANCE_ALPHA. Since U,V bytes are interspread
                //in the texture, this is probably the fastest way to use them in the shader
                "   u = texture2D(uv_texture, v_texCoord).a - 0.5;  \n" +
                "   v = texture2D(uv_texture, v_texCoord).r - 0.5;  \n" +


                //The numbers are just YUV to RGB conversion constants
                "   r = y + 1.13983*v;                              \n" +
                "   g = y - 0.39465*u - 0.58060*v;                  \n" +
                "   b = y + 2.03211*u;                              \n" +

                //We finally set the RGB color of our pixel
                "   gl_FragColor = vec4(r, g, b, 1.0);              \n" +
                "}                                                  \n";
        private static final float[] VERTEX = {   // in counterclockwise order:
                1, 1, 0,   // top right
                -1, 1, 0,  // top left
                -1, -1, 0, // bottom left
                1, -1, 0,  // bottom right
        };
        private static final short[] VERTEX_INDEX = { 0, 1, 2, 2, 0, 3 };
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
//        private int mTexSamplerHandle;
        private int mYTextSampleHandle;
        private int mUVTextSampleHandle;

        protected int mCameraWidth;
        protected int mCameraHeight;

        private int mWidth;
        private int mHeight;
        private int[] mTexNames;

        private byte[] image;
        private byte[] image2;

        private Camera camera; //The camera object

        //The Y and UV buffers that will pass our image channel data to the textures
        private ByteBuffer yBuffer;
        private ByteBuffer uvBuffer;

        private int[] yTextureNames;
        private int[] uvTextureNames;
        SurfaceViewActivity mSurfaceViewActivity;

        private GLSurfaceView mView;
        private DrawView mDrawView;
        private TextView mTextView;

        long mDrawTimeStamp = 0;
        long mTrackTimeStamp = 0;
        int mDrawFrame = 0;
        int mTrackFrame = 0;
        int mDrawFps = 0;
        int mTrackFps = 0;
        private Boolean isInitTrack = false;
        Handler mHandler = new Handler(Looper.getMainLooper());

        public static final int MAX_WIDTH = 800;
        public static final int MAX_HEIGHT = 800;


        MyRenderer(final SurfaceViewActivity activity) {
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


            activity.mUnusedTextureView.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
                @Override
                public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
                    camera = Camera.open();
                    try {
                        camera.setPreviewTexture(mSurfaceViewActivity.mUnusedTextureView.getSurfaceTexture());
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                    //Set the first buffer, the preview doesn't start unless we set the buffers

                    Camera.Parameters params = camera.getParameters();
                    params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
                    for(Camera.Size size : params.getSupportedPreviewSizes()){
                        if(size.width<= MAX_WIDTH && size.height <= MAX_HEIGHT){
                            mCameraHeight = size.height;
                            mCameraWidth = size.width;
                            params.setPreviewSize(mCameraWidth,mCameraHeight);
                            break;
                        }
                    }
                    camera.setParameters(params);

                    image = new byte[mCameraHeight*mCameraWidth*12/8];
                    image2 = new byte[mCameraHeight*mCameraWidth*12/8];
                    yBuffer = ByteBuffer.allocate(mCameraWidth*mCameraHeight).order(ByteOrder.nativeOrder());
                    uvBuffer = ByteBuffer.allocate(mCameraWidth*mCameraHeight/2).order(ByteOrder.nativeOrder());

                    camera.addCallbackBuffer(image);
                    camera.addCallbackBuffer(image2);
                    camera.setPreviewCallbackWithBuffer(MyRenderer.this);

                    initView();

                    camera.startPreview();
                }

                @Override
                public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {

                }

                @Override
                public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
                    return false;
                }

                @Override
                public void onSurfaceTextureUpdated(SurfaceTexture surface) {

                }
            });

        }

        void initView(){
            int textureWidth = UtilMethod.getScreenWidth();
            int textureHeight = UtilMethod.getScreenWidth() * mCameraWidth / mCameraHeight;
            mView = mSurfaceViewActivity.mGLSurfaceView;
            mView.setLayoutParams(new RelativeLayout.LayoutParams(
                    textureWidth, textureHeight));

            if(mDrawView == null){
                mDrawView = (DrawView) ((View)mView.getParent()).findViewById(R.id.obj_drawview);
                mTextView = (TextView) ((View)mView.getParent()).findViewById(R.id.obj_infoview);
                mDrawView.setDrawCallback(new DrawView.CaluateViewCallBack() {
                    @Override
                    public void onCaluate(RectF rectF) {
                        isInitTrack = false;
                    }
                });
            }
            mDrawView.setLayoutParams(new RelativeLayout.LayoutParams(textureWidth, textureHeight));
            mDrawView.setCameraSize(mCameraWidth, mCameraHeight);
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
//            mTexSamplerHandle = GLES20.glGetUniformLocation(mProgram, "s_texture");
            mYTextSampleHandle = GLES20.glGetUniformLocation(mProgram, "y_texture");
            mUVTextSampleHandle = GLES20.glGetUniformLocation(mProgram, "uv_texture");

//            mTexNames = new int[1];
//            GLES20.glGenTextures(1, mTexNames, 0);
//
//            Bitmap bitmap = BitmapFactory.decodeResource(mResources, R.mipmap.bumpy_bricks_public_domain);
//            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
//            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, mTexNames[0]);
//            GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bitmap, 0);
//            bitmap.recycle();

            GLES20.glEnable(GLES20.GL_TEXTURE_2D);
            yTextureNames = new int[1];
            GLES20.glGenTextures(1, yTextureNames, 0);

            GLES20.glEnable(GLES20.GL_TEXTURE_2D);
            uvTextureNames = new int[1];
            GLES20.glGenTextures(1, uvTextureNames, 0);
        }

        @Override
        public void onSurfaceChanged(GL10 unused, int width, int height) {
            mWidth = width;
            mHeight = height;



            GLES20.glViewport(0, 0, width, height);
            // Create a new perspective projection matrix. The height will stay the same
            // while the width will vary as per aspect ratio.
            final float ratio = (float) width / height;

//            Matrix.setIdentityM(mModelMatrix, 0);
//            Matrix.translateM(mModelMatrix, 0, 0, 0f, 0f);
//            Matrix.rotateM(mModelMatrix, 0, -90, 0f, 0f, 1f);

            Matrix.frustumM(mProjectionMatrix, 0, -1, 1, -1, 1, 3, 7);

//            Matrix.rotateM(mMVPMatrix, 0, -90, 0f, 0f, 1f);
//            Matrix.translateM(mMVPMatrix, 0, 0, -1, 0);
        }

        @Override
        public void onDrawFrame(GL10 unused) {
            Matrix.setLookAtM(mCameraMatrix, 0, 0, 0, 3f, 0, 0, 0, -1, 0, 0);
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
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yTextureNames[0]);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, mCameraWidth, mCameraHeight, 0,
                    GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, yBuffer);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,
                    GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,
                    GLES20.GL_REPEAT);
            GLES20.glUniform1i(mYTextSampleHandle, 0);


            GLES20.glActiveTexture(GLES20.GL_TEXTURE1);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, uvTextureNames[0]);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE_ALPHA, mCameraWidth/2, mCameraHeight/2,
                    0, GLES20.GL_LUMINANCE_ALPHA, GLES20.GL_UNSIGNED_BYTE, uvBuffer);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,
                    GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,
                    GLES20.GL_REPEAT);
            GLES20.glUniform1i(mUVTextSampleHandle,1);


            GLES20.glUniformMatrix4fv(mMatrixHandle, 1, false, mMVPMatrix, 0);

            GLES20.glDrawElements(GLES20.GL_TRIANGLES, VERTEX_INDEX.length,
                    GLES20.GL_UNSIGNED_SHORT, mVertexIndexBuffer);

            GLES20.glDisableVertexAttribArray(mPositionHandle);
            GLES20.glDisableVertexAttribArray(mTexCoordHandle);



        }

        void destroy() {
            GLES20.glDeleteTextures(1, yTextureNames, 0);
            GLES20.glDeleteTextures(1,uvTextureNames,0);
        }

        static int loadShader(int type, String shaderCode) {
            int shader = GLES20.glCreateShader(type);
            GLES20.glShaderSource(shader, shaderCode);
            GLES20.glCompileShader(shader);
            return shader;
        }


        byte[] currentData;
        Thread trackThread = new Thread(){
            @Override
            public void run() {
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
                while (currentData != null){
                    long startTimeStamp = System.currentTimeMillis();
                    mTrackFrame++;
                    synchronized (isInitTrack) {
                        float rateY = mDrawView.getWidth() * 1.0f / mCameraHeight;
                        float rateX = mDrawView.getHeight() * 1.0f / mCameraWidth;
                        RectF rectF = new RectF(mDrawView.mDrawRectF);
                        rectF.left = rectF.left / rateX;
                        rectF.right = rectF.right / rateX;
                        rectF.top = rectF.top / rateY;
                        rectF.bottom = rectF.bottom / rateY;
                        if (!isInitTrack) {
                            ObjTrack.openTrack(currentData, ObjTrack.TYPE_NV21,(int) rectF.left, (int) rectF.top, (int) rectF.width(), (int) rectF.height(), mCameraWidth, mCameraHeight);
                            isInitTrack = true;
                            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_MORE_FAVORABLE);
                        } else {
                            int[] cmtData = ObjTrack.processTrack(currentData,ObjTrack.TYPE_NV21,mCameraWidth, mCameraHeight);
                            if(cmtData != null){
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
        };

        @Override
        public void onPreviewFrame(byte[] data, Camera camera) {
            try{
                if (mDrawView.mDrawRectF == null) {
                    return;
                }
                currentData = data;
                if(!trackThread.isAlive()){
                    trackThread.start();
                }
            }finally {
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
                yBuffer.put(data,0,mCameraWidth*mCameraHeight).position(0);
                uvBuffer.put(data,mCameraWidth*mCameraHeight,mCameraWidth*mCameraHeight/2).position(0);
                camera.addCallbackBuffer(data);
                updateFps();
            }
        }
    }
}
