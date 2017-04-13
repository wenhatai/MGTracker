package com.melonteam.objtrack;

import android.app.Application;
import android.content.res.AssetManager;
import android.os.Environment;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Created by zhangpengyu on 2017/3/20.
 */

public class AppAplication extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        ObjTrack.init();
        new Thread(){
            @Override
            public void run() {
                copyAssets();
            }
        }.start();
    }


    private void copyAssets() {
        AssetManager assetManager = getAssets();
        File dirFile = getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS);
        if(!dirFile.exists()){
            dirFile.mkdirs();
        }
        String[] files = null;
        try {
            files = assetManager.list("");
        } catch (IOException e) {
            Log.e("tag", "Failed to get asset file list.", e);
        }
        if (files != null) for (String filename : files) {
            InputStream in = null;
            OutputStream out = null;
            try {
                File outFile = new File(getExternalFilesDir(null), filename);
                if(outFile.exists()){
                    continue;
                }
                in = assetManager.open(filename);
                out = new FileOutputStream(outFile);
                copyFile(in, out);
            } catch(IOException e) {
                Log.e("tag", "Failed to copy asset file: " + filename, e);
            }
            finally {
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
            }
        }
    }


    private void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while((read = in.read(buffer)) != -1){
            out.write(buffer, 0, read);
        }
    }
}
