package com.melonteam.objtrack;

import android.content.Context;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.RelativeLayout;
import android.widget.TextView;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.opengles.GL10;

/**
 * Created by zhangpengyu on 2017/3/20.
 */
@Deprecated
public class MyGLSurfaceView2 extends GLSurfaceView {

    MainRenderer mRenderer;

    public MyGLSurfaceView2(Context context ) {
        super (context);
        init();
    }

    public MyGLSurfaceView2(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    protected void init(){
        mRenderer = new MainRenderer(this);
        setEGLContextClientVersion ( 2 );
        setRenderer ( mRenderer );
        setRenderMode ( GLSurfaceView.RENDERMODE_WHEN_DIRTY );
    }

    public void surfaceCreated ( SurfaceHolder holder ) {
        super.surfaceCreated ( holder );

    }

    public void surfaceDestroyed ( SurfaceHolder holder ) {
        mRenderer.close();
        super.surfaceDestroyed ( holder );
    }



    public void surfaceChanged ( SurfaceHolder holder, int format, int w, int h ) {

        super.surfaceChanged ( holder, format, w, h );
    }

    public static class MainRenderer implements Renderer, SurfaceTexture.OnFrameAvailableListener,Camera.PreviewCallback{
        private final String vss =
                "attribute vec2 vPosition;\n" +
                        "attribute vec2 vTexCoord;\n" +
                        "varying vec2 texCoord;\n" +
                        "void main() {\n" +
                        "  texCoord = vTexCoord;\n" +
                        "  gl_Position = vec4 ( vPosition.x, vPosition.y, 0.0, 1.0 );\n" +
                        "}";

        private final String fss =
                "#extension GL_OES_EGL_image_external : require\n" +
                        "precision mediump float;\n" +
                        "uniform samplerExternalOES sTexture;\n" +
                        "varying vec2 texCoord;\n" +
                        "void main() {\n" +
                        "  gl_FragColor = texture2D(sTexture,texCoord);\n" +
                        "}";

        private int[] hTex;
        private FloatBuffer pVertex;
        private FloatBuffer pTexCoord;
        private int hProgram;

        private Camera mCamera;
        private SurfaceTexture mSTexture;

        private boolean mUpdateST = false;

        private MyGLSurfaceView2 mView;
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

        MainRenderer (MyGLSurfaceView2 view ) {
            mView = view;
            float[] vtmp = { 1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
            float[] ttmp = { 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
            pVertex = ByteBuffer.allocateDirect(8*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            pVertex.put ( vtmp );
            pVertex.position(0);
            pTexCoord = ByteBuffer.allocateDirect(8*4).order(ByteOrder.nativeOrder()).asFloatBuffer();
            pTexCoord.put ( ttmp );
            pTexCoord.position(0);

        }

        public void close()
        {
            mUpdateST = false;
            if(mSTexture != null){
                mSTexture.setOnFrameAvailableListener(null);
            }
            mSTexture.release();
            mCamera.setPreviewCallback(null);
            mCamera.stopPreview();
            mCamera.release();
            mCamera = null;
            deleteTex();
        }

        public void onDrawFrame ( GL10 unused ) {
            GLES20.glClear( GLES20.GL_COLOR_BUFFER_BIT );

            synchronized(this) {
                if ( mUpdateST ) {
                    mSTexture.updateTexImage();
                    mUpdateST = false;
                }
            }

            GLES20.glUseProgram(hProgram);

            int ph = GLES20.glGetAttribLocation(hProgram, "vPosition");
            int tch = GLES20.glGetAttribLocation ( hProgram, "vTexCoord" );
            int th = GLES20.glGetUniformLocation ( hProgram, "sTexture" );

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, hTex[0]);
            GLES20.glUniform1i(th, 0);

            GLES20.glVertexAttribPointer(ph, 2, GLES20.GL_FLOAT, false, 4*2, pVertex);
            GLES20.glVertexAttribPointer(tch, 2, GLES20.GL_FLOAT, false, 4*2, pTexCoord );
            GLES20.glEnableVertexAttribArray(ph);
            GLES20.glEnableVertexAttribArray(tch);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            GLES20.glFlush();

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

        int mCameraWidth = 0;
        int mCameraHeight = 0;

        @Override
        public void onSurfaceCreated(GL10 gl, javax.microedition.khronos.egl.EGLConfig config) {
            initTex();
            mSTexture = new SurfaceTexture ( hTex[0] );
            mSTexture.setOnFrameAvailableListener(this);

            mCamera = Camera.open();
            mCamera.setPreviewCallback(this);
            try {
                mCamera.setPreviewTexture(mSTexture);
            } catch ( IOException ioe ) {
            }

            GLES20.glClearColor ( 1.0f, 1.0f, 0.0f, 1.0f );

            hProgram = loadShader ( vss, fss );
        }

        public void onSurfaceChanged (GL10 unused, int width, int height ) {
            GLES20.glViewport( 0, 0, width, height );
            Camera.Parameters param = mCamera.getParameters();
            for (Camera.Size size : param.getSupportedPictureSizes()) {
                if (size.width <= 800 && size.height <= 800) {
                    mCameraHeight = size.height;
                    mCameraWidth = size.width;
                    break;
                }
            }
            initView();

            param.set("orientation", "landscape");
            mCamera.setParameters ( param );
            mCamera.startPreview();
        }


        private void initView() {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
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
                    int textureWidth = GLSurfaceViewActivity2.ScreentWidth;
                    int textureHeight = textureWidth * mCameraWidth / mCameraHeight;
                    mView.setLayoutParams(new RelativeLayout.LayoutParams(
                            textureWidth, textureHeight));
                    mDrawView.setLayoutParams(new RelativeLayout.LayoutParams(textureWidth, textureHeight));
                    mDrawView.setCameraSize(mCameraWidth, mCameraHeight);

                }
            });
        }

        protected void updateFps(){
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    if(isInitTrack){
                        mTextView.setText("绘制fps:"+mDrawFps+",追踪fps:"+mTrackFps);
                    }else{
                        mTextView.setText("");
                    }
                }
            });
        }


        private void initTex() {
            hTex = new int[1];
            GLES20.glGenTextures ( 1, hTex, 0 );
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, hTex[0]);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
            GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_NEAREST);
        }

        private void deleteTex() {
            GLES20.glDeleteTextures ( 1, hTex, 0 );
        }

        public synchronized void onFrameAvailable ( SurfaceTexture st ) {
            mUpdateST = true;
            mView.requestRender();
        }

        private static int loadShader ( String vss, String fss ) {
            int vshader = GLES20.glCreateShader(GLES20.GL_VERTEX_SHADER);
            GLES20.glShaderSource(vshader, vss);
            GLES20.glCompileShader(vshader);
            int[] compiled = new int[1];
            GLES20.glGetShaderiv(vshader, GLES20.GL_COMPILE_STATUS, compiled, 0);
            if (compiled[0] == 0) {
                Log.e("Shader", "Could not compile vshader");
                Log.v("Shader", "Could not compile vshader:"+GLES20.glGetShaderInfoLog(vshader));
                GLES20.glDeleteShader(vshader);
                vshader = 0;
            }

            int fshader = GLES20.glCreateShader(GLES20.GL_FRAGMENT_SHADER);
            GLES20.glShaderSource(fshader, fss);
            GLES20.glCompileShader(fshader);
            GLES20.glGetShaderiv(fshader, GLES20.GL_COMPILE_STATUS, compiled, 0);
            if (compiled[0] == 0) {
                Log.e("Shader", "Could not compile fshader");
                Log.v("Shader", "Could not compile fshader:"+GLES20.glGetShaderInfoLog(fshader));
                GLES20.glDeleteShader(fshader);
                fshader = 0;
            }

            int program = GLES20.glCreateProgram();
            GLES20.glAttachShader(program, vshader);
            GLES20.glAttachShader(program, fshader);
            GLES20.glLinkProgram(program);

            return program;
        }

        @Override
        public void onPreviewFrame(byte[] data, Camera camera) {
            if (mDrawView.mDrawRectF == null) {
                return;
            }
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
                    ObjTrack.openTrack(data, (int) rectF.left, (int) rectF.top, (int) rectF.width(), (int) rectF.height(), mCameraWidth, mCameraHeight);
                    isInitTrack = true;
                } else {
                    int[] cmtData = ObjTrack.processTrack(data,mCameraWidth, mCameraHeight);
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
}
