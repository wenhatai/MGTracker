package com.melonteam.mgtracker;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PointF;
import android.graphics.RectF;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

/**
 * Created by zhangpengyu on 2017/3/20.
 */

public class DrawView extends View {
    PointF mStartPoint;
    PointF mEndPoint;
    Paint mPaint;
    RectF mRectF;
    RectF mDrawRectF;//tracker 比例
    float mRateX;
    float mRateY;
    CaluateViewCallBack mCallBack;
    int mCameraWidth;
    int mCameraHeight;
    public static final int CameraMode = 1;
    public static final int VideoMode = 2;

    public int mViewMode = CameraMode;

    public DrawView(Context context) {
        super(context);
        init();
    }

    public DrawView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public DrawView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public interface CaluateViewCallBack{
        public void onCaluate(RectF rectF);
    }

    public void setCameraSize(int cameraWidth,int cameraHeight){
        this.mCameraHeight = cameraHeight;
        this.mCameraWidth = cameraWidth;
    }

    public void setDrawCallback(CaluateViewCallBack callback){
        this.mCallBack = callback;
    }

    protected void init(){
        mStartPoint = new PointF();
        mEndPoint = new PointF();
        mPaint = new Paint();
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setStrokeWidth(2);
        mPaint.setColor(Color.WHITE);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()){
            case MotionEvent.ACTION_DOWN:
                mRectF = null;
                if(mDrawRectF != null){
                    synchronized (mDrawRectF){
                        mDrawRectF = null;
                    }
                }
                mStartPoint.x =  event.getX();
                mStartPoint.y = event.getY();
                break;
            case MotionEvent.ACTION_MOVE:
                mEndPoint.x = event.getX();
                mEndPoint.y = event.getY();
                caluateRectF();
                invalidate();
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                mEndPoint.x = event.getX();
                mEndPoint.y = event.getY();
                caluateRectF();
                onCallBack();
                invalidate();
                break;
        }
        return true;
    }

    protected void caluateRectF(){
        float left,right,top,bottom;
        if(mStartPoint.x < mEndPoint.x){
            left = mStartPoint.x;
            right = mEndPoint.x;
        }else{
            left = mEndPoint.x;
            right = mStartPoint.x;
        }
        if(mStartPoint.y < mEndPoint.y){
            top = mStartPoint.y;
            bottom = mEndPoint.y;
        }else{
            top = mEndPoint.y;
            bottom = mStartPoint.y;
        }
        if(left == right || top == bottom){
            return;
        }
        mRateX = mCameraWidth*1.0f/this.getWidth();
        mRateY = mCameraHeight*1.0f/this.getHeight();
        mRectF = new RectF(left,top,right,bottom);
    }

    public void onCallBack(){
        if(mDrawRectF == null){
            mDrawRectF = new RectF();
        }
        synchronized (mDrawRectF){
            if(mViewMode == CameraMode){
                int viewHeight = getWidth();
                mDrawRectF.top = viewHeight - mRectF.right;
                mDrawRectF.left = mRectF.top;
                mDrawRectF.bottom = viewHeight - mRectF.left;
                mDrawRectF.right = mRectF.bottom;
            }else{
                mDrawRectF = mRectF;
            }
            if(mCallBack != null){
                mCallBack.onCaluate(mDrawRectF);
            }
        }
    }

    protected RectF getDrawRectF(){
        RectF rectF = new RectF();
        if(mViewMode == CameraMode){
            int viewHeight = getWidth();
            rectF.left = viewHeight - mDrawRectF.bottom;
            rectF.top = mDrawRectF.left;
            rectF.right = viewHeight - mDrawRectF.top;
            rectF.bottom = mDrawRectF.right;
        }else{
            rectF = mDrawRectF;
        }
        return rectF;
    }


    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if(mDrawRectF != null){
            canvas.drawRect(getDrawRectF(), mPaint);
            return;
        }
        if(mRectF != null){
            canvas.drawRect(mRectF, mPaint);
        }
    }
}
