package com.melonteam.objtrack;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.Bundle;
import android.view.TextureView;
import android.widget.RelativeLayout;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by zhangpengyu on 2017/4/1.
 * yuv 转 rgb后一直是黑白的，用另外一种换算公式则会偏绿
 */

@Deprecated
public class YUVActivity extends Activity {
    /**
     * Hold a reference to our GLSurfaceView
     */
    private GLSurfaceView mGLSurfaceView;
    private TextureView mUnusedTextureView;
    MyRenderer mRender;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_surfaceview);
        mGLSurfaceView = (GLSurfaceView) findViewById(R.id.obj_surfaceview);
        mUnusedTextureView = (TextureView) findViewById(R.id.unused_texture);


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

    static class MyRenderer implements GLSurfaceView.Renderer, Camera.PreviewCallback {
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
                "uniform sampler2D v_texture;                      \n"+
                "uniform sampler2D rgb_texture;\n"+

                "void main (void){                                  \n" +
                "   float r, g, b, y, u, v;                         \n" +

//                "   y = texture2D(y_texture, v_texCoord).r;         \n" +
//
//                "   u = texture2D(u_texture, v_texCoord).r;  \n" +
//                "   v = texture2D(v_texture, v_texCoord).r;  " +

//                "r = 1.164*(y-0.0625)+1.596*(v-0.5);" +
//                "g = 1.164*(y-0.0625)-0.813*(v-0.5)-0.391*(u-0.5);" +
//                "b = 1.164*(y-0.0625)+2.018*(u-0.5);"+

//                "   r = y + 1.13983*v;                              \n" +
//                "   g = y - 0.39465*u - 0.58060*v;                  \n" +
//                "   b = y + 2.03211*u;                              \n" +
                "r = texture2D(rgb_texture, v_texCoord).r;\n"+
                "g = texture2D(rgb_texture, v_texCoord).g;\n"+
                "b = texture2D(rgb_texture, v_texCoord).b;\n"+
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
        private int mYTextSampleHandle;
        private int mUTextSampleHandle;
        private int mVTextSampleHandle;

        protected int mCameraWidth;
        protected int mCameraHeight;

        private static byte[] image;

        private Camera camera; //The camera object

        private ByteBuffer yBuffer;
        private ByteBuffer uBuffer;
        private ByteBuffer vBuffer;

        private int[] yTextureNames;
        private int[] uTextureNames;
        private int[] vTextureNames;
        YUVActivity mSurfaceViewActivity;


        MyRenderer(final YUVActivity activity) {
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
                    for (Camera.Size size : params.getSupportedPreviewSizes()) {
                        if (size.width <= 800 && size.height <= 800) {
                            mCameraHeight = size.height;
                            mCameraWidth = size.width;
                            params.setPreviewSize(mCameraWidth, mCameraHeight);
                            break;
                        }
                    }
                    params.setPreviewFormat(ImageFormat.YV12);
                    camera.setParameters(params);

                    image = new byte[mCameraHeight * mCameraWidth / 8 * 12];
                    yBuffer = ByteBuffer.allocate(mCameraWidth * mCameraHeight).order(ByteOrder.nativeOrder());
                    uBuffer = ByteBuffer.allocate(mCameraWidth * mCameraHeight /4).order(ByteOrder.nativeOrder());
                    vBuffer = ByteBuffer.allocate(mCameraWidth*mCameraHeight/4).order(ByteOrder.nativeOrder());

                    camera.addCallbackBuffer(image);
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

        void initView() {
            int textureWidth = MainActivity.getScreenWidth();
            int textureHeight = MainActivity.getScreenWidth() * mCameraWidth / mCameraHeight;
            mSurfaceViewActivity.mGLSurfaceView.setLayoutParams(new RelativeLayout.LayoutParams(
                    textureWidth, textureHeight));
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
            mYTextSampleHandle = GLES20.glGetUniformLocation(mProgram, "y_texture");
            mUTextSampleHandle = GLES20.glGetUniformLocation(mProgram, "u_texture");
            mVTextSampleHandle = GLES20.glGetUniformLocation(mProgram,"v_texture");


            GLES20.glEnable(GLES20.GL_TEXTURE_2D);
            yTextureNames = new int[1];
            GLES20.glGenTextures(1, yTextureNames, 0);

            GLES20.glEnable(GLES20.GL_TEXTURE_2D);
            uTextureNames = new int[1];
            GLES20.glGenTextures(1, uTextureNames, 0);

            GLES20.glEnable(GLES20.GL_TEXTURE_2D);
            vTextureNames = new int[1];
            GLES20.glGenTextures(1,vTextureNames,0);
        }

        @Override
        public void onSurfaceChanged(GL10 unused, int width, int height) {

            // Create a new perspective projection matrix. The height will stay the same
            // while the width will vary as per aspect ratio.
            final float ratio = (float) width / height;

            Matrix.setIdentityM(mModelMatrix, 0);
//            Matrix.translateM(mModelMatrix, 0, 0f, -0.2f, 0f);
            Matrix.rotateM(mModelMatrix, 0, -90, 0f, 0f, 1f);


            Matrix.frustumM(mProjectionMatrix, 0, -ratio, ratio, -1, 1, 3, 7);
            Matrix.setLookAtM(mCameraMatrix, 0, 0, 0, 3f, 0, 0, 0, 0, 1, 0);
            Matrix.multiplyMM(mMVPMatrix, 0, mCameraMatrix, 0, mModelMatrix, 0);
            Matrix.multiplyMM(mMVPMatrix, 0, mProjectionMatrix, 0, mMVPMatrix, 0);
//            Matrix.rotateM(mMVPMatrix, 0, -90, 0f, 0f, 1f);
//            Matrix.translateM(mMVPMatrix, 0, 0, -1, 0);
        }

        @Override
        public void onDrawFrame(GL10 unused) {
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
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, vTextureNames[0]);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, mCameraWidth / 2, mCameraHeight / 2,
                    0, GLES20.GL_LUMINANCE, GLES20.GL_LUMINANCE, vBuffer);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,
                    GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,
                    GLES20.GL_REPEAT);
            GLES20.glUniform1i(mVTextSampleHandle, 1);

            GLES20.glActiveTexture(GLES20.GL_TEXTURE2);
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, uTextureNames[0]);
            GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, mCameraWidth / 2, mCameraHeight / 2,
                    0, GLES20.GL_LUMINANCE, GLES20.GL_LUMINANCE, uBuffer);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER,
                    GLES20.GL_LINEAR);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,
                    GLES20.GL_REPEAT);
            GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,
                    GLES20.GL_REPEAT);
            GLES20.glUniform1i(mUTextSampleHandle, 2);


            GLES20.glUniformMatrix4fv(mMatrixHandle, 1, false, mMVPMatrix, 0);

            GLES20.glDrawElements(GLES20.GL_TRIANGLES, VERTEX_INDEX.length,
                    GLES20.GL_UNSIGNED_SHORT, mVertexIndexBuffer);

            GLES20.glDisableVertexAttribArray(mPositionHandle);
            GLES20.glDisableVertexAttribArray(mTexCoordHandle);

        }

        void destroy() {
            GLES20.glDeleteTextures(1, yTextureNames, 0);
            GLES20.glDeleteTextures(1, uTextureNames, 0);
            GLES20.glDeleteTextures(1,vTextureNames,0);
        }

        static int loadShader(int type, String shaderCode) {
            int shader = GLES20.glCreateShader(type);
            GLES20.glShaderSource(shader, shaderCode);
            GLES20.glCompileShader(shader);
            return shader;
        }

        @Override
        public void onPreviewFrame(byte[] data, Camera camera) {
            yBuffer.put(data, 0, mCameraWidth * mCameraHeight).position(0);
            vBuffer.put(data, mCameraWidth * mCameraHeight, mCameraWidth * mCameraHeight / 4).position(0);
            uBuffer.put(data, mCameraWidth * mCameraHeight*5/4, mCameraWidth * mCameraHeight / 4).position(0);
            camera.addCallbackBuffer(data);
        }
    }
}