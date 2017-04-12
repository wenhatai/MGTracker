package com.melonteam.objtrack;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import pub.devrel.easypermissions.AppSettingsDialog;
import pub.devrel.easypermissions.EasyPermissions;

/**
 * Created by zhangpengyu on 2017/4/6.
 */

public class ListActivity extends Activity implements EasyPermissions.PermissionCallbacks{
    /**
     * 请求CAMERA权限码
     */
    public static final int REQUEST_CAMERA_PERM = 101;
    public static final String[] Datas = new String[]{"摄像头追踪","镜头移动前景固定","背景固定前景移动",
            "视频追踪-Basketball","视频追踪-DragonBady","视频追踪-MotorRolling","视频追踪-Bolt2"};

    public static final String[] FileNames = new String[]{"jjcmmv.mp4","jjbkfix.mp4",
            "basketball.mp4","DragonBady.mp4", "MotorRolling.mp4","Bolt2.mp4" };

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ListView listView = new ListView(this);
        List<String> dataList = new ArrayList<>();
        for(int i = 0;i< Datas.length;i++){
            dataList.add(Datas[i]);
        }
        listView.setAdapter(new ArrayAdapter<String>(this, android.R.layout.simple_expandable_list_item_1,dataList));
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if(position == 0){
                    if (EasyPermissions.hasPermissions(ListActivity.this, Manifest.permission.CAMERA)) {
                        startCamerActivity();
                    } else{
                        EasyPermissions.requestPermissions(ListActivity.this, "需要请求camera权限",
                                REQUEST_CAMERA_PERM, Manifest.permission.CAMERA);
                    }
                }else{
                    Intent intent = new Intent(ListActivity.this,VideoActivity.class);
                    File outFile = new File(getExternalFilesDir(null), FileNames[position-1]);
                    intent.putExtra("FilePath",outFile.getAbsolutePath());
                    startActivity(intent);
                }

            }
        });
        setContentView(listView);
    }

    protected void startCamerActivity(){
        Intent intent = new Intent(ListActivity.this,CameraActivity.class);
        startActivity(intent);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        EasyPermissions.onRequestPermissionsResult(requestCode, permissions, grantResults, this);
    }


    @Override
    public void onPermissionsGranted(int requestCode, List<String> perms) {
        startCamerActivity();
    }

    @Override
    public void onPermissionsDenied(int requestCode, List<String> perms) {
        if (EasyPermissions.somePermissionPermanentlyDenied(this, perms)) {
            new AppSettingsDialog.Builder(this, "当前App需要申请camera权限,需要打开设置页面么?")
                    .setTitle("权限申请")
                    .setPositiveButton("确认")
                    .setNegativeButton("取消", null /* click listener */)
                    .setRequestCode(REQUEST_CAMERA_PERM)
                    .build()
                    .show();
        }
    }
}
